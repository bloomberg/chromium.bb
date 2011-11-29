// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile.h"

#include "build/build_config.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/speech/chrome_speech_input_preferences.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/bookmark_load_observer.h"
#include "chrome/test/base/test_url_request_context_getter.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/mock_resource_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/database/database_tracker.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/quota/mock_quota_manager.h"
#include "webkit/quota/quota_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif  // defined(OS_CHROMEOS)

using base::Time;
using content::BrowserThread;
using testing::NiceMock;
using testing::Return;

namespace {

// Task used to make sure history has finished processing a request. Intended
// for use with BlockUntilHistoryProcessesPendingRequests.

class QuittingHistoryDBTask : public HistoryDBTask {
 public:
  QuittingHistoryDBTask() {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    return true;
  }

  virtual void DoneRunOnMainThread() {
    MessageLoop::current()->Quit();
  }

 private:
  ~QuittingHistoryDBTask() {}

  DISALLOW_COPY_AND_ASSIGN(QuittingHistoryDBTask);
};

class TestExtensionURLRequestContext : public net::URLRequestContext {
 public:
  TestExtensionURLRequestContext() {
    net::CookieMonster* cookie_monster = new net::CookieMonster(NULL, NULL);
    const char* schemes[] = {chrome::kExtensionScheme};
    cookie_monster->SetCookieableSchemes(schemes, 1);
    set_cookie_store(cookie_monster);
  }
};

class TestExtensionURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestExtensionURLRequestContext();
    return context_.get();
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  }

 private:
  scoped_refptr<net::URLRequestContext> context_;
};

ProfileKeyedService* CreateTestDesktopNotificationService(Profile* profile) {
  return new DesktopNotificationService(profile, NULL);
}

}  // namespace

TestingProfile::TestingProfile()
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      incognito_(false),
      last_session_exited_cleanly_(true),
      profile_dependency_manager_(ProfileDependencyManager::GetInstance()) {
  if (!temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create unique temporary directory.";

    // Fallback logic in case we fail to create unique temporary directory.
    FilePath system_tmp_dir;
    bool success = PathService::Get(base::DIR_TEMP, &system_tmp_dir);

    // We're severly screwed if we can't get the system temporary
    // directory. Die now to avoid writing to the filesystem root
    // or other bad places.
    CHECK(success);

    FilePath fallback_dir(system_tmp_dir.AppendASCII("TestingProfilePath"));
    file_util::Delete(fallback_dir, true);
    file_util::CreateDirectory(fallback_dir);
    if (!temp_dir_.Set(fallback_dir)) {
      // That shouldn't happen, but if it does, try to recover.
      LOG(ERROR) << "Failed to use a fallback temporary directory.";

      // We're screwed if this fails, see CHECK above.
      CHECK(temp_dir_.Set(system_tmp_dir));
    }
  }

  profile_path_ = temp_dir_.path();

  Init();
}

TestingProfile::TestingProfile(const FilePath& path)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      incognito_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      profile_dependency_manager_(ProfileDependencyManager::GetInstance()) {
  Init();
}

void TestingProfile::Init() {
  profile_dependency_manager_->CreateProfileServices(this, true);

  // Install profile keyed service factory hooks for dummy/test services
  DesktopNotificationServiceFactory::GetInstance()->SetTestingFactory(
      this, CreateTestDesktopNotificationService);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(static_cast<Profile*>(this)),
      content::NotificationService::NoDetails());
}

TestingProfile::~TestingProfile() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::Source<Profile>(static_cast<Profile*>(this)),
      content::NotificationService::NoDetails());

  profile_dependency_manager_->DestroyProfileServices(this);

  if (host_content_settings_map_)
    host_content_settings_map_->ShutdownOnUIThread();

  DestroyTopSites();
  DestroyHistoryService();
  // FaviconService depends on HistoryServce so destroying it later.
  DestroyFaviconService();
  DestroyWebDataService();

  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();

  // Close the handles so that proper cleanup can be done.
  db_tracker_ = NULL;
}

void TestingProfile::CreateFaviconService() {
  favicon_service_.reset(new FaviconService(this));
}

void TestingProfile::CreateHistoryService(bool delete_file, bool no_db) {
  DestroyHistoryService();
  if (delete_file) {
    FilePath path = GetPath();
    path = path.Append(chrome::kHistoryFilename);
    file_util::Delete(path, false);
  }
  history_service_ = new HistoryService(this);
  history_service_->Init(GetPath(), bookmark_bar_model_.get(), no_db);
}

void TestingProfile::DestroyHistoryService() {
  if (!history_service_.get())
    return;

  history_service_->NotifyRenderProcessHostDestruction(0);
  history_service_->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
  history_service_->Cleanup();
  history_service_ = NULL;

  // Wait for the backend class to terminate before deleting the files and
  // moving to the next test. Note: if this never terminates, somebody is
  // probably leaking a reference to the history backend, so it never calls
  // our destroy task.
  MessageLoop::current()->Run();

  // Make sure we don't have any event pending that could disrupt the next
  // test.
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  MessageLoop::current()->Run();
}

void TestingProfile::CreateTopSites() {
  DestroyTopSites();
  top_sites_ = new history::TopSites(this);
  FilePath file_name = GetPath().Append(chrome::kTopSitesFilename);
  top_sites_->Init(file_name);
}

void TestingProfile::DestroyTopSites() {
  if (top_sites_.get()) {
    top_sites_->Shutdown();
    top_sites_ = NULL;
    // TopSites::Shutdown schedules some tasks (from TopSitesBackend) that need
    // to be run to properly shutdown. Run all pending tasks now. This is
    // normally handled by browser_process shutdown.
    if (MessageLoop::current())
      MessageLoop::current()->RunAllPending();
  }
}

void TestingProfile::DestroyFaviconService() {
  favicon_service_.reset();
}

void TestingProfile::CreateBookmarkModel(bool delete_file) {
  // Nuke the model first, that way we're sure it's done writing to disk.
  bookmark_bar_model_.reset(NULL);

  if (delete_file) {
    FilePath path = GetPath();
    path = path.Append(chrome::kBookmarksFileName);
    file_util::Delete(path, false);
  }
  bookmark_bar_model_.reset(new BookmarkModel(this));
  if (history_service_.get()) {
    history_service_->history_backend_->bookmark_service_ =
        bookmark_bar_model_.get();
    history_service_->history_backend_->expirer_.bookmark_service_ =
        bookmark_bar_model_.get();
  }
  bookmark_bar_model_->Load();
}

void TestingProfile::CreateAutocompleteClassifier() {
  autocomplete_classifier_.reset(new AutocompleteClassifier(this));
}

void TestingProfile::CreateProtocolHandlerRegistry() {
  protocol_handler_registry_ = new ProtocolHandlerRegistry(this,
      new ProtocolHandlerRegistry::Delegate());
}

void TestingProfile::CreateWebDataService(bool delete_file) {
  if (web_data_service_.get())
    web_data_service_->Shutdown();

  if (delete_file) {
    FilePath path = GetPath();
    path = path.Append(chrome::kWebDataFilename);
    file_util::Delete(path, false);
  }

  web_data_service_ = new WebDataService;
  if (web_data_service_.get())
    web_data_service_->Init(GetPath());
}

void TestingProfile::BlockUntilBookmarkModelLoaded() {
  DCHECK(bookmark_bar_model_.get());
  if (bookmark_bar_model_->IsLoaded())
    return;
  BookmarkLoadObserver observer;
  bookmark_bar_model_->AddObserver(&observer);
  MessageLoop::current()->Run();
  bookmark_bar_model_->RemoveObserver(&observer);
  DCHECK(bookmark_bar_model_->IsLoaded());
}

// TODO(phajdan.jr): Doesn't this hang if Top Sites are already loaded?
void TestingProfile::BlockUntilTopSitesLoaded() {
  ui_test_utils::WindowedNotificationObserver top_sites_loaded_observer(
      chrome::NOTIFICATION_TOP_SITES_LOADED,
      content::NotificationService::AllSources());
  if (!GetHistoryService(Profile::EXPLICIT_ACCESS))
    GetTopSites()->HistoryLoaded();
  top_sites_loaded_observer.Wait();
}

void TestingProfile::CreateTemplateURLFetcher() {
  template_url_fetcher_.reset(new TemplateURLFetcher(this));
}

static ProfileKeyedService* BuildTemplateURLService(Profile* profile) {
  return new TemplateURLService(profile);
}

void TestingProfile::CreateTemplateURLService() {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      this, BuildTemplateURLService);
}

void TestingProfile::BlockUntilTemplateURLServiceLoaded() {
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(this);
  if (turl_model->loaded())
    return;

  ui_test_utils::WindowedNotificationObserver turl_service_load_observer(
      chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
      content::NotificationService::AllSources());
  turl_model->Load();
  turl_service_load_observer.Wait();
}

void TestingProfile::CreateExtensionProcessManager() {
  extension_process_manager_.reset(ExtensionProcessManager::Create(this));
}

ExtensionService* TestingProfile::CreateExtensionService(
    const CommandLine* command_line,
    const FilePath& install_directory,
    bool autoupdate_enabled) {
  // Extension pref store, created for use by |extension_prefs_|.

  extension_pref_value_map_.reset(new ExtensionPrefValueMap);

  bool extensions_disabled =
      command_line && command_line->HasSwitch(switches::kDisableExtensions);

  // Note that the GetPrefs() creates a TestingPrefService, therefore
  // the extension controlled pref values set in extension_prefs_
  // are not reflected in the pref service. One would need to
  // inject a new ExtensionPrefStore(extension_pref_value_map_.get(), false).
  extension_prefs_.reset(
      new ExtensionPrefs(GetPrefs(),
                         install_directory,
                         extension_pref_value_map_.get()));
  extension_prefs_->Init(extensions_disabled);
  extension_service_.reset(new ExtensionService(this,
                                                command_line,
                                                install_directory,
                                                extension_prefs_.get(),
                                                autoupdate_enabled,
                                                true));
  return extension_service_.get();
}

FilePath TestingProfile::GetPath() {
  return profile_path_;
}

TestingPrefService* TestingProfile::GetTestingPrefService() {
  if (!prefs_.get())
    CreateTestingPrefService();
  DCHECK(testing_prefs_);
  return testing_prefs_;
}

TestingProfile* TestingProfile::AsTestingProfile() {
  return this;
}

std::string TestingProfile::GetProfileName() {
  return std::string("testing_profile");
}

bool TestingProfile::IsOffTheRecord() {
  return incognito_;
}

void TestingProfile::SetOffTheRecordProfile(Profile* profile) {
  incognito_profile_.reset(profile);
}

Profile* TestingProfile::GetOffTheRecordProfile() {
  return incognito_profile_.get();
}

GAIAInfoUpdateService* TestingProfile::GetGAIAInfoUpdateService() {
  return NULL;
}

bool TestingProfile::HasOffTheRecordProfile() {
  return incognito_profile_.get() != NULL;
}

Profile* TestingProfile::GetOriginalProfile() {
  return this;
}

void TestingProfile::SetAppCacheService(
    ChromeAppCacheService* appcache_service) {
  appcache_service_ = appcache_service;
}

ChromeAppCacheService* TestingProfile::GetAppCacheService() {
  return appcache_service_.get();
}

webkit_database::DatabaseTracker* TestingProfile::GetDatabaseTracker() {
  if (!db_tracker_) {
    db_tracker_ = new webkit_database::DatabaseTracker(
        GetPath(), false, false, GetExtensionSpecialStoragePolicy(),
        NULL, NULL);
  }
  return db_tracker_;
}

VisitedLinkMaster* TestingProfile::GetVisitedLinkMaster() {
  return NULL;
}

ExtensionService* TestingProfile::GetExtensionService() {
  return extension_service_.get();
}

UserScriptMaster* TestingProfile::GetUserScriptMaster() {
  return NULL;
}

ExtensionDevToolsManager* TestingProfile::GetExtensionDevToolsManager() {
  return NULL;
}

ExtensionProcessManager* TestingProfile::GetExtensionProcessManager() {
  return extension_process_manager_.get();
}

ExtensionMessageService* TestingProfile::GetExtensionMessageService() {
  return NULL;
}

ExtensionEventRouter* TestingProfile::GetExtensionEventRouter() {
  return NULL;
}

void TestingProfile::SetExtensionSpecialStoragePolicy(
    ExtensionSpecialStoragePolicy* extension_special_storage_policy) {
  extension_special_storage_policy_ = extension_special_storage_policy;
}

ExtensionSpecialStoragePolicy*
TestingProfile::GetExtensionSpecialStoragePolicy() {
  if (!extension_special_storage_policy_.get())
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy(NULL);
  return extension_special_storage_policy_.get();
}

SSLHostState* TestingProfile::GetSSLHostState() {
  return NULL;
}

FaviconService* TestingProfile::GetFaviconService(ServiceAccessType access) {
  return favicon_service_.get();
}

HistoryService* TestingProfile::GetHistoryService(ServiceAccessType access) {
  return history_service_.get();
}

HistoryService* TestingProfile::GetHistoryServiceWithoutCreating() {
  return history_service_.get();
}

net::CookieMonster* TestingProfile::GetCookieMonster() {
  if (!GetRequestContext())
    return NULL;
  return GetRequestContext()->GetURLRequestContext()->cookie_store()->
      GetCookieMonster();
}

AutocompleteClassifier* TestingProfile::GetAutocompleteClassifier() {
  return autocomplete_classifier_.get();
}

history::ShortcutsBackend* TestingProfile::GetShortcutsBackend() {
  return NULL;
}

WebDataService* TestingProfile::GetWebDataService(ServiceAccessType access) {
  return web_data_service_.get();
}

WebDataService* TestingProfile::GetWebDataServiceWithoutCreating() {
  return web_data_service_.get();
}

PasswordStore* TestingProfile::GetPasswordStore(ServiceAccessType access) {
  return NULL;
}

void TestingProfile::SetPrefService(PrefService* prefs) {
  DCHECK(!prefs_.get());
  prefs_.reset(prefs);
}

void TestingProfile::CreateTestingPrefService() {
  DCHECK(!prefs_.get());
  testing_prefs_ = new TestingPrefService();
  prefs_.reset(testing_prefs_);
  Profile::RegisterUserPrefs(prefs_.get());
  browser::RegisterUserPrefs(prefs_.get());
}

PrefService* TestingProfile::GetPrefs() {
  if (!prefs_.get()) {
    CreateTestingPrefService();
  }
  return prefs_.get();
}

TemplateURLFetcher* TestingProfile::GetTemplateURLFetcher() {
  return template_url_fetcher_.get();
}

history::TopSites* TestingProfile::GetTopSites() {
  return top_sites_.get();
}

history::TopSites* TestingProfile::GetTopSitesWithoutCreating() {
  return top_sites_.get();
}

DownloadManager* TestingProfile::GetDownloadManager() {
  return NULL;
}

fileapi::FileSystemContext* TestingProfile::GetFileSystemContext() {
  if (!file_system_context_) {
    file_system_context_ = new fileapi::FileSystemContext(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      GetExtensionSpecialStoragePolicy(),
      NULL,
      GetPath(),
      IsOffTheRecord(),
      true,  // Allow file access from files.
      NULL);
  }
  return file_system_context_.get();
}

void TestingProfile::SetQuotaManager(quota::QuotaManager* manager) {
  quota_manager_ = manager;
}

quota::QuotaManager* TestingProfile::GetQuotaManager() {
  return quota_manager_.get();
}

net::URLRequestContextGetter* TestingProfile::GetRequestContext() {
  return request_context_.get();
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  if (extension_service_.get()) {
    const Extension* installed_app = extension_service_->
        GetInstalledAppForRenderer(renderer_child_id);
    if (installed_app != NULL && installed_app->is_storage_isolated())
      return GetRequestContextForIsolatedApp(installed_app->id());
  }

  return GetRequestContext();
}

void TestingProfile::CreateRequestContext() {
  if (!request_context_)
    request_context_ = new TestURLRequestContextGetter();
}

void TestingProfile::ResetRequestContext() {
  request_context_ = NULL;
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForMedia() {
  return NULL;
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForExtensions() {
  if (!extensions_request_context_)
      extensions_request_context_ = new TestExtensionURLRequestContextGetter();
  return extensions_request_context_.get();
}

net::SSLConfigService* TestingProfile::GetSSLConfigService() {
  return NULL;
}

UserStyleSheetWatcher* TestingProfile::GetUserStyleSheetWatcher() {
  return NULL;
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForIsolatedApp(
    const std::string& app_id) {
  // We don't test isolated app storage here yet, so returning the same dummy
  // context is sufficient for now.
  return GetRequestContext();
}

const content::ResourceContext& TestingProfile::GetResourceContext() {
  return *content::MockResourceContext::GetInstance();
}

FindBarState* TestingProfile::GetFindBarState() {
  if (!find_bar_state_.get())
    find_bar_state_.reset(new FindBarState());
  return find_bar_state_.get();
}

HostContentSettingsMap* TestingProfile::GetHostContentSettingsMap() {
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(
        GetPrefs(), GetExtensionService(), false);
  }
  return host_content_settings_map_.get();
}

GeolocationPermissionContext*
TestingProfile::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_.get()) {
    geolocation_permission_context_ =
        new ChromeGeolocationPermissionContext(this);
  }
  return geolocation_permission_context_.get();
}

SpeechInputPreferences* TestingProfile::GetSpeechInputPreferences() {
  if (!speech_input_preferences_.get())
    speech_input_preferences_ = new ChromeSpeechInputPreferences(GetPrefs());
  return speech_input_preferences_.get();
}

HostZoomMap* TestingProfile::GetHostZoomMap() {
  return NULL;
}

bool TestingProfile::HasProfileSyncService() const {
  return (profile_sync_service_.get() != NULL);
}

std::wstring TestingProfile::GetName() {
  return std::wstring();
}

std::wstring TestingProfile::GetID() {
  return id_;
}

void TestingProfile::SetID(const std::wstring& id) {
  id_ = id;
}

bool TestingProfile::DidLastSessionExitCleanly() {
  return last_session_exited_cleanly_;
}

BookmarkModel* TestingProfile::GetBookmarkModel() {
  return bookmark_bar_model_.get();
}

bool TestingProfile::IsSameProfile(Profile *p) {
  return this == p;
}

base::Time TestingProfile::GetStartTime() const {
  return start_time_;
}

ProtocolHandlerRegistry* TestingProfile::GetProtocolHandlerRegistry() {
  return protocol_handler_registry_.get();
}

SpellCheckHost* TestingProfile::GetSpellCheckHost() {
  return NULL;
}

WebKitContext* TestingProfile::GetWebKitContext() {
  if (webkit_context_ == NULL) {
    webkit_context_ = new WebKitContext(
          IsOffTheRecord(), GetPath(),
          GetExtensionSpecialStoragePolicy(),
          false, NULL, NULL);
  }
  return webkit_context_;
}

WebKitContext* TestingProfile::GetOffTheRecordWebKitContext() {
  return NULL;
}

FilePath TestingProfile::last_selected_directory() {
  return last_selected_directory_;
}

void TestingProfile::set_last_selected_directory(const FilePath& path) {
  last_selected_directory_ = path;
}

PrefProxyConfigTracker* TestingProfile::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_.get()) {
    pref_proxy_config_tracker_.reset(
        ProxyServiceFactory::CreatePrefProxyConfigTracker(GetPrefs()));
  }
  return pref_proxy_config_tracker_.get();
}

void TestingProfile::BlockUntilHistoryProcessesPendingRequests() {
  DCHECK(history_service_.get());
  DCHECK(MessageLoop::current());

  CancelableRequestConsumer consumer;
  history_service_->ScheduleDBTask(new QuittingHistoryDBTask(), &consumer);
  MessageLoop::current()->Run();
}

TokenService* TestingProfile::GetTokenService() {
  if (!token_service_.get()) {
    token_service_.reset(new TokenService());
  }
  return token_service_.get();
}

ProfileSyncService* TestingProfile::GetProfileSyncService() {
  return GetProfileSyncService("");
}

ProfileSyncService* TestingProfile::GetProfileSyncService(
    const std::string& cros_user) {
  if (!profile_sync_service_.get()) {
    // Use a NiceMock here since we are really using the mock as a
    // fake.  Test cases that want to set expectations on a
    // ProfileSyncService should use the ProfileMock and have this
    // method return their own mock instance.
    profile_sync_service_.reset(new NiceMock<ProfileSyncServiceMock>());
  }
  return profile_sync_service_.get();
}

ChromeBlobStorageContext* TestingProfile::GetBlobStorageContext() {
  return NULL;
}

ExtensionInfoMap* TestingProfile::GetExtensionInfoMap() {
  return NULL;
}

PromoCounter* TestingProfile::GetInstantPromoCounter() {
  return NULL;
}

ChromeURLDataManager* TestingProfile::GetChromeURLDataManager() {
  if (!chrome_url_data_manager_.get())
    chrome_url_data_manager_.reset(
        new ChromeURLDataManager(
            base::Callback<ChromeURLDataManagerBackend*(void)>()));
  return chrome_url_data_manager_.get();
}

chrome_browser_net::Predictor* TestingProfile::GetNetworkPredictor() {
  return NULL;
}

void TestingProfile::ClearNetworkingHistorySince(base::Time time) {
  NOTIMPLEMENTED();
}

GURL TestingProfile::GetHomePage() {
  return GURL(chrome::kChromeUINewTabURL);
}

NetworkActionPredictor* TestingProfile::GetNetworkActionPredictor() {
  return NULL;
}

PrefService* TestingProfile::GetOffTheRecordPrefs() {
  return NULL;
}

quota::SpecialStoragePolicy* TestingProfile::GetSpecialStoragePolicy() {
  return GetExtensionSpecialStoragePolicy();
}

void TestingProfile::DestroyWebDataService() {
  if (!web_data_service_.get())
    return;

  web_data_service_->Shutdown();
}

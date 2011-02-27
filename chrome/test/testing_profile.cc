// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_profile.h"

#include "build/build_config.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/ntp_resource_cache.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/test_url_request_context_getter.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/database/database_tracker.h"

#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#endif

using base::Time;
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

// BookmarkLoadObserver is used when blocking until the BookmarkModel
// finishes loading. As soon as the BookmarkModel finishes loading the message
// loop is quit.
class BookmarkLoadObserver : public BookmarkModelObserver {
 public:
  BookmarkLoadObserver() {}
  virtual void Loaded(BookmarkModel* model) {
    MessageLoop::current()->Quit();
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkLoadObserver);
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

class TestExtensionURLRequestContextGetter : public URLRequestContextGetter {
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

}  // namespace

TestingProfile::TestingProfile()
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      created_theme_provider_(false),
      has_history_service_(false),
      off_the_record_(false),
      last_session_exited_cleanly_(true) {
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
}

TestingProfile::~TestingProfile() {
  NotificationService::current()->Notify(
      NotificationType::PROFILE_DESTROYED,
      Source<Profile>(this),
      NotificationService::NoDetails());
  DestroyTopSites();
  DestroyHistoryService();
  // FaviconService depends on HistoryServce so destroying it later.
  DestroyFaviconService();
  DestroyWebDataService();
  if (extensions_service_.get()) {
    extensions_service_->DestroyingProfile();
    extensions_service_ = NULL;
  }
  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();
}

void TestingProfile::CreateFaviconService() {
  favicon_service_ = NULL;
  favicon_service_ = new FaviconService(this);
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
  if (!favicon_service_.get())
    return;
  favicon_service_ = NULL;
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
  protocol_handler_registry_ = new ProtocolHandlerRegistry(this);
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

void TestingProfile::BlockUntilTopSitesLoaded() {
  if (!GetHistoryService(Profile::EXPLICIT_ACCESS))
    GetTopSites()->HistoryLoaded();
  ui_test_utils::WaitForNotification(NotificationType::TOP_SITES_LOADED);
}

void TestingProfile::CreateTemplateURLFetcher() {
  template_url_fetcher_.reset(new TemplateURLFetcher(this));
}

void TestingProfile::CreateTemplateURLModel() {
  SetTemplateURLModel(new TemplateURLModel(this));
}

void TestingProfile::SetTemplateURLModel(TemplateURLModel* model) {
  template_url_model_.reset(model);
}

void TestingProfile::UseThemeProvider(BrowserThemeProvider* theme_provider) {
  theme_provider->Init(this);
  created_theme_provider_ = true;
  theme_provider_.reset(theme_provider);
}

ExtensionService* TestingProfile::CreateExtensionService(
    const CommandLine* command_line,
    const FilePath& install_directory) {
  // Extension pref store, created for use by |extension_prefs_|.

  extension_pref_value_map_.reset(new ExtensionPrefValueMap);
  // Note that the GetPrefs() creates a TestingPrefService, therefore
  // the extension controlled pref values set in extension_prefs_
  // are not reflected in the pref service. One would need to
  // inject a new ExtensionPrefStore(extension_pref_value_map_.get(), false).
  extension_prefs_.reset(
      new ExtensionPrefs(GetPrefs(),
                         install_directory,
                         extension_pref_value_map_.get()));
  extensions_service_ = new ExtensionService(this,
                                             command_line,
                                             install_directory,
                                             extension_prefs_.get(),
                                             false);
  return extensions_service_;
}

FilePath TestingProfile::GetPath() {
  DCHECK(temp_dir_.IsValid());  // TODO(phajdan.jr): do it better.
  return temp_dir_.path();
}

TestingPrefService* TestingProfile::GetTestingPrefService() {
  if (!prefs_.get())
    CreateTestingPrefService();
  DCHECK(testing_prefs_);
  return testing_prefs_;
}

ProfileId TestingProfile::GetRuntimeId() {
    return reinterpret_cast<ProfileId>(this);
  }

bool TestingProfile::IsOffTheRecord() {
  return off_the_record_;
}

void TestingProfile::SetOffTheRecordProfile(Profile* profile) {
  off_the_record_profile_.reset(profile);
}

Profile* TestingProfile::GetOffTheRecordProfile() {
  return off_the_record_profile_.get();
}

bool TestingProfile::HasOffTheRecordProfile() {
  return off_the_record_profile_.get() != NULL;
}

Profile* TestingProfile::GetOriginalProfile() {
  return this;
}

ChromeAppCacheService* TestingProfile::GetAppCacheService() {
  return NULL;
}

webkit_database::DatabaseTracker* TestingProfile::GetDatabaseTracker() {
  if (!db_tracker_) {
    db_tracker_ = new webkit_database::DatabaseTracker(
        GetPath(), false, GetExtensionSpecialStoragePolicy());
  }
  return db_tracker_;
}

VisitedLinkMaster* TestingProfile::GetVisitedLinkMaster() {
  return NULL;
}

ExtensionService* TestingProfile::GetExtensionService() {
  return extensions_service_.get();
}

UserScriptMaster* TestingProfile::GetUserScriptMaster() {
  return NULL;
}

ExtensionDevToolsManager* TestingProfile::GetExtensionDevToolsManager() {
  return NULL;
}

ExtensionProcessManager* TestingProfile::GetExtensionProcessManager() {
  return NULL;
}

ExtensionMessageService* TestingProfile::GetExtensionMessageService() {
  return NULL;
}

ExtensionEventRouter* TestingProfile::GetExtensionEventRouter() {
  return NULL;
}

ExtensionIOEventRouter* TestingProfile::GetExtensionIOEventRouter() {
  return NULL;
}

ExtensionSpecialStoragePolicy*
TestingProfile::GetExtensionSpecialStoragePolicy() {
  if (!extension_special_storage_policy_)
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy();
  return extension_special_storage_policy_;
}

SSLHostState* TestingProfile::GetSSLHostState() {
  return NULL;
}

net::TransportSecurityState* TestingProfile::GetTransportSecurityState() {
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
  return GetRequestContext()->GetCookieStore()->GetCookieMonster();
}

AutocompleteClassifier* TestingProfile::GetAutocompleteClassifier() {
  return autocomplete_classifier_.get();
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

void TestingProfile::InitThemes() {
  if (!created_theme_provider_) {
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
    theme_provider_.reset(new GtkThemeProvider);
#else
    theme_provider_.reset(new BrowserThemeProvider);
#endif
    theme_provider_->Init(this);
    created_theme_provider_ = true;
  }
}

const Extension* TestingProfile::GetTheme() {
  return NULL;
}

BrowserThemeProvider* TestingProfile::GetThemeProvider() {
  InitThemes();
  return theme_provider_.get();
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

TemplateURLModel* TestingProfile::GetTemplateURLModel() {
  return template_url_model_.get();
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

PersonalDataManager* TestingProfile::GetPersonalDataManager() {
  return NULL;
}

fileapi::FileSystemContext* TestingProfile::GetFileSystemContext() {
  return NULL;
}

BrowserSignin* TestingProfile::GetBrowserSignin() {
  return NULL;
}

bool TestingProfile::HasCreatedDownloadManager() const {
  return false;
}

URLRequestContextGetter* TestingProfile::GetRequestContext() {
  return request_context_.get();
}

void TestingProfile::CreateRequestContext() {
  if (!request_context_)
    request_context_ = new TestURLRequestContextGetter();
}

void TestingProfile::ResetRequestContext() {
  request_context_ = NULL;
}

URLRequestContextGetter* TestingProfile::GetRequestContextForMedia() {
  return NULL;
}

URLRequestContextGetter* TestingProfile::GetRequestContextForExtensions() {
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

FindBarState* TestingProfile::GetFindBarState() {
  if (!find_bar_state_.get())
    find_bar_state_.reset(new FindBarState());
  return find_bar_state_.get();
}

HostContentSettingsMap* TestingProfile::GetHostContentSettingsMap() {
  if (!host_content_settings_map_.get())
    host_content_settings_map_ = new HostContentSettingsMap(this);
  return host_content_settings_map_.get();
}

GeolocationContentSettingsMap*
TestingProfile::GetGeolocationContentSettingsMap() {
  if (!geolocation_content_settings_map_.get()) {
    geolocation_content_settings_map_ =
        new GeolocationContentSettingsMap(this);
  }
  return geolocation_content_settings_map_.get();
}

GeolocationPermissionContext*
TestingProfile::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_.get()) {
    geolocation_permission_context_ =
        new GeolocationPermissionContext(this);
  }
  return geolocation_permission_context_.get();
}

HostZoomMap* TestingProfile::GetHostZoomMap() {
  return NULL;
}

SessionService* TestingProfile::GetSessionService() {
  return session_service_.get();
}

bool TestingProfile::HasSessionService() const {
  return (session_service_.get() != NULL);
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

TabRestoreService* TestingProfile::GetTabRestoreService() {
  return NULL;
}

ProtocolHandlerRegistry* TestingProfile::GetProtocolHandlerRegistry() {
  return protocol_handler_registry_.get();
}

SpellCheckHost* TestingProfile::GetSpellCheckHost() {
  return NULL;
}

void TestingProfile::set_session_service(SessionService* session_service) {
  session_service_ = session_service;
}

WebKitContext* TestingProfile::GetWebKitContext() {
  if (webkit_context_ == NULL)
    webkit_context_ = new WebKitContext(this, false);
  return webkit_context_;
}

WebKitContext* TestingProfile::GetOffTheRecordWebKitContext() {
  return NULL;
}

NTPResourceCache* TestingProfile::GetNTPResourceCache() {
  if (!ntp_resource_cache_.get())
    ntp_resource_cache_.reset(new NTPResourceCache(this));
  return ntp_resource_cache_.get();
}

DesktopNotificationService* TestingProfile::GetDesktopNotificationService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!desktop_notification_service_.get()) {
    desktop_notification_service_.reset(new DesktopNotificationService(
        this, NULL));
  }
  return desktop_notification_service_.get();
}

BackgroundContentsService*
TestingProfile::GetBackgroundContentsService() const {
  return NULL;
}

StatusTray* TestingProfile::GetStatusTray() {
  return NULL;
}

FilePath TestingProfile::last_selected_directory() {
  return last_selected_directory_;
}

void TestingProfile::set_last_selected_directory(const FilePath& path) {
  last_selected_directory_ = path;
}

PrefProxyConfigTracker* TestingProfile::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_)
    pref_proxy_config_tracker_ = new PrefProxyConfigTracker(GetPrefs());

  return pref_proxy_config_tracker_;
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

CloudPrintProxyService* TestingProfile::GetCloudPrintProxyService() {
  return NULL;
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

policy::ProfilePolicyConnector* TestingProfile::GetPolicyConnector() {
  return NULL;
}

ChromeURLDataManager* TestingProfile::GetChromeURLDataManager() {
  if (!chrome_url_data_manager_.get())
    chrome_url_data_manager_.reset(new ChromeURLDataManager(this));
  return chrome_url_data_manager_.get();
}

prerender::PrerenderManager* TestingProfile::GetPrerenderManager() {
  return NULL;
}

PrefService* TestingProfile::GetOffTheRecordPrefs() {
  return NULL;
}

void TestingProfile::DestroyWebDataService() {
  if (!web_data_service_.get())
    return;

  web_data_service_->Shutdown();
}

DerivedTestingProfile::DerivedTestingProfile(Profile* profile)
    : original_profile_(profile) {}

DerivedTestingProfile::~DerivedTestingProfile() {}

ProfileId DerivedTestingProfile::GetRuntimeId() {
  return original_profile_->GetRuntimeId();
}

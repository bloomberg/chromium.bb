// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile.h"

#include "build/build_config.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/history/shortcuts_backend_factory.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/speech/chrome_speech_recognition_preferences.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/bookmark_load_observer.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/mock_resource_context.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/policy_service_impl.h"
#else
#include "chrome/browser/policy/policy_service_stub.h"
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif  // defined(OS_CHROMEOS)

using base::Time;
using content::BrowserThread;
using content::DownloadManagerDelegate;
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

  virtual ~TestExtensionURLRequestContext() {}
};

class TestExtensionURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_.get())
      context_.reset(new TestExtensionURLRequestContext());
    return context_.get();
  }
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  }

 protected:
  virtual ~TestExtensionURLRequestContextGetter() {}

 private:
  scoped_ptr<net::URLRequestContext> context_;
};

ProfileKeyedService* CreateTestDesktopNotificationService(Profile* profile) {
#if defined(ENABLE_NOTIFICATIONS)
  return new DesktopNotificationService(profile, NULL);
#else
  return NULL;
#endif
}

}  // namespace

TestingProfile::TestingProfile()
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      incognito_(false),
      last_session_exited_cleanly_(true),
      profile_dependency_manager_(ProfileDependencyManager::GetInstance()),
      delegate_(NULL) {
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
  FinishInit();
}

TestingProfile::TestingProfile(const FilePath& path)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      incognito_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      profile_dependency_manager_(ProfileDependencyManager::GetInstance()),
      delegate_(NULL) {
  Init();
  FinishInit();
}

TestingProfile::TestingProfile(const FilePath& path,
                               Delegate* delegate)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      incognito_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      profile_dependency_manager_(ProfileDependencyManager::GetInstance()),
      delegate_(delegate) {
  Init();
  if (delegate_) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(&TestingProfile::FinishInit,
                                                base::Unretained(this)));
  } else {
    FinishInit();
  }
}

void TestingProfile::Init() {
  if (!file_util::PathExists(profile_path_))
    file_util::CreateDirectory(profile_path_);

  ExtensionSystemFactory::GetInstance()->SetTestingFactory(
      this, TestExtensionSystem::Build);

  profile_dependency_manager_->CreateProfileServices(this, true);

#if defined(ENABLE_NOTIFICATIONS)
  // Install profile keyed service factory hooks for dummy/test services
  DesktopNotificationServiceFactory::GetInstance()->SetTestingFactory(
      this, CreateTestDesktopNotificationService);
#endif
}

void TestingProfile::FinishInit() {
  DCHECK(content::NotificationService::current());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(static_cast<Profile*>(this)),
      content::NotificationService::NoDetails());

  if (delegate_)
    delegate_->OnProfileCreated(this, true, false);
}

TestingProfile::~TestingProfile() {
  DCHECK(content::NotificationService::current());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::Source<Profile>(static_cast<Profile*>(this)),
      content::NotificationService::NoDetails());

  profile_dependency_manager_->DestroyProfileServices(this);

  if (host_content_settings_map_)
    host_content_settings_map_->ShutdownOnUIThread();

  DestroyTopSites();
  DestroyFaviconService();

  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();
}

void TestingProfile::CreateFaviconService() {
  favicon_service_.reset(new FaviconService(this));
}

static scoped_refptr<RefcountedProfileKeyedService> BuildHistoryService(
    Profile* profile) {
  return new HistoryService(profile);
}

void TestingProfile::CreateHistoryService(bool delete_file, bool no_db) {
  DestroyHistoryService();
  if (delete_file) {
    FilePath path = GetPath();
    path = path.Append(chrome::kHistoryFilename);
    file_util::Delete(path, false);
  }
  // This will create and init the history service.
  HistoryService* history_service = static_cast<HistoryService*>(
      HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          this, BuildHistoryService).get());
  if (!history_service->Init(this->GetPath(),
                             BookmarkModelFactory::GetForProfile(this),
                             no_db)) {
    HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(this, NULL);
  }
}

void TestingProfile::DestroyHistoryService() {
  scoped_refptr<HistoryService> history_service =
      HistoryServiceFactory::GetForProfileIfExists(this);
  if (!history_service.get())
    return;

  history_service->NotifyRenderProcessHostDestruction(0);
  history_service->SetOnBackendDestroyTask(MessageLoop::QuitClosure());
  history_service->Cleanup();
  HistoryServiceFactory::ShutdownForProfile(this);

  // Wait for the backend class to terminate before deleting the files and
  // moving to the next test. Note: if this never terminates, somebody is
  // probably leaking a reference to the history backend, so it never calls
  // our destroy task.
  MessageLoop::current()->Run();

  // Make sure we don't have any event pending that could disrupt the next
  // test.
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
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

static ProfileKeyedService* BuildBookmarkModel(Profile* profile) {
  BookmarkModel* bookmark_model = new BookmarkModel(profile);
  bookmark_model->Load();
  return bookmark_model;
}


void TestingProfile::CreateBookmarkModel(bool delete_file) {

  if (delete_file) {
    FilePath path = GetPath();
    path = path.Append(chrome::kBookmarksFileName);
    file_util::Delete(path, false);
  }
  // This will create a bookmark model.
  BookmarkModel* bookmark_service =
      static_cast<BookmarkModel*>(
          BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
              this, BuildBookmarkModel));

  HistoryService* history_service =
      HistoryServiceFactory::GetForProfileIfExists(this).get();
  if (history_service) {
    history_service->history_backend_->bookmark_service_ =
        bookmark_service;
    history_service->history_backend_->expirer_.bookmark_service_ =
        bookmark_service;
  }
}

void TestingProfile::CreateProtocolHandlerRegistry() {
  protocol_handler_registry_ = new ProtocolHandlerRegistry(this,
      new ProtocolHandlerRegistry::Delegate());
}

static scoped_refptr<RefcountedProfileKeyedService> BuildWebDataService(
    Profile* profile) {
  WebDataService* web_data_service = new WebDataService();
  if (web_data_service)
    web_data_service->Init(profile->GetPath());
  return web_data_service;
}

void TestingProfile::CreateWebDataService() {
  WebDataServiceFactory::GetInstance()->SetTestingFactory(
      this, BuildWebDataService);
}

void TestingProfile::BlockUntilBookmarkModelLoaded() {
  DCHECK(GetBookmarkModel());
  if (GetBookmarkModel()->IsLoaded())
    return;
  base::RunLoop run_loop;
  BookmarkLoadObserver observer(
      ui_test_utils::GetQuitTaskForRunLoop(&run_loop));
  GetBookmarkModel()->AddObserver(&observer);
  run_loop.Run();
  GetBookmarkModel()->RemoveObserver(&observer);
  DCHECK(GetBookmarkModel()->IsLoaded());
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

bool TestingProfile::IsOffTheRecord() const {
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

VisitedLinkMaster* TestingProfile::GetVisitedLinkMaster() {
  return NULL;
}

ExtensionService* TestingProfile::GetExtensionService() {
  return ExtensionSystem::Get(this)->extension_service();
}

UserScriptMaster* TestingProfile::GetUserScriptMaster() {
  return ExtensionSystem::Get(this)->user_script_master();
}

ExtensionProcessManager* TestingProfile::GetExtensionProcessManager() {
  return ExtensionSystem::Get(this)->process_manager();
}

ExtensionEventRouter* TestingProfile::GetExtensionEventRouter() {
  return ExtensionSystem::Get(this)->event_router();
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

FaviconService* TestingProfile::GetFaviconService(ServiceAccessType access) {
  return favicon_service_.get();
}

HistoryService* TestingProfile::GetHistoryService(ServiceAccessType access) {
  return HistoryServiceFactory::GetForProfileIfExists(this);
}

HistoryService* TestingProfile::GetHistoryServiceWithoutCreating() {
  return HistoryServiceFactory::GetForProfileIfExists(this);
}

net::CookieMonster* TestingProfile::GetCookieMonster() {
  if (!GetRequestContext())
    return NULL;
  return GetRequestContext()->GetURLRequestContext()->cookie_store()->
      GetCookieMonster();
}

policy::PolicyService* TestingProfile::GetPolicyService() {
  if (!policy_service_.get()) {
#if defined(ENABLE_CONFIGURATION_POLICY)
    policy::PolicyServiceImpl::Providers providers;
    policy_service_.reset(new policy::PolicyServiceImpl(providers));
#else
    policy_service_.reset(new policy::PolicyServiceStub());
#endif
  }
  return policy_service_.get();
}

void TestingProfile::SetPrefService(PrefService* prefs) {
#if defined(ENABLE_PROTECTOR_SERVICE)
  // ProtectorService binds itself very closely to the PrefService at the moment
  // of Profile creation and watches pref changes to update their backup.
  // For tests that replace the PrefService after TestingProfile creation,
  // ProtectorService is disabled to prevent further invalid memory accesses.
  protector::ProtectorServiceFactory::GetInstance()->
      SetTestingFactory(this, NULL);
#endif
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

history::TopSites* TestingProfile::GetTopSites() {
  return top_sites_.get();
}

history::TopSites* TestingProfile::GetTopSitesWithoutCreating() {
  return top_sites_.get();
}

DownloadManagerDelegate* TestingProfile::GetDownloadManagerDelegate() {
  return NULL;
}

net::URLRequestContextGetter* TestingProfile::GetRequestContext() {
  return request_context_.get();
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  ExtensionService* extension_service =
      ExtensionSystem::Get(this)->extension_service();
  if (extension_service) {
    const extensions::Extension* installed_app = extension_service->
        GetInstalledAppForRenderer(renderer_child_id);
    if (installed_app != NULL && installed_app->is_storage_isolated())
      return GetRequestContextForIsolatedApp(installed_app->id());
  }

  return GetRequestContext();
}

void TestingProfile::CreateRequestContext() {
  if (!request_context_)
    request_context_ =
        new TestURLRequestContextGetter(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
}

void TestingProfile::ResetRequestContext() {
  // Any objects holding live URLFetchers should be deleted before the request
  // context is shut down.
  TemplateURLFetcherFactory::ShutdownForProfile(this);

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

net::URLRequestContextGetter* TestingProfile::GetRequestContextForIsolatedApp(
    const std::string& app_id) {
  // We don't test isolated app storage here yet, so returning the same dummy
  // context is sufficient for now.
  return GetRequestContext();
}

content::ResourceContext* TestingProfile::GetResourceContext() {
  if (!resource_context_.get())
    resource_context_.reset(new content::MockResourceContext());
  return resource_context_.get();
}

HostContentSettingsMap* TestingProfile::GetHostContentSettingsMap() {
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(GetPrefs(), false);
    ExtensionService* extension_service = GetExtensionService();
    if (extension_service)
      host_content_settings_map_->RegisterExtensionService(extension_service);
  }
  return host_content_settings_map_.get();
}

content::GeolocationPermissionContext*
TestingProfile::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_.get()) {
    geolocation_permission_context_ =
        new ChromeGeolocationPermissionContext(this);
  }
  return geolocation_permission_context_.get();
}

content::SpeechRecognitionPreferences*
    TestingProfile::GetSpeechRecognitionPreferences() {
#if defined(ENABLE_INPUT_SPEECH)
  return ChromeSpeechRecognitionPreferences::GetForProfile(this);
#else
  return NULL;
#endif
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
  return BookmarkModelFactory::GetForProfileIfExists(this);
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
  scoped_refptr<HistoryService> history_service =
      HistoryServiceFactory::GetForProfileIfExists(this);
  DCHECK(history_service.get());
  DCHECK(MessageLoop::current());

  CancelableRequestConsumer consumer;
  history_service->ScheduleDBTask(new QuittingHistoryDBTask(), &consumer);
  MessageLoop::current()->Run();
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

PrefService* TestingProfile::GetOffTheRecordPrefs() {
  return NULL;
}

quota::SpecialStoragePolicy* TestingProfile::GetSpecialStoragePolicy() {
  return GetExtensionSpecialStoragePolicy();
}

bool TestingProfile::WasCreatedByVersionOrLater(const std::string& version) {
  return true;
}

base::Callback<ChromeURLDataManagerBackend*(void)>
    TestingProfile::GetChromeURLDataManagerBackendGetter() const {
  return base::Callback<ChromeURLDataManagerBackend*(void)>();
}

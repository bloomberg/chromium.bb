// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/prefs/testing_pref_store.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/favicon/chrome_favicon_client_factory.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/history/chrome_history_client_factory.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/history_index_restore_observer.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema.h"
#else
#include "components/policy/core/common/policy_service_stub.h"
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/extension_system.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using base::Time;
using content::BrowserThread;
using content::DownloadManagerDelegate;
using testing::NiceMock;
using testing::Return;

namespace {

// Task used to make sure history has finished processing a request. Intended
// for use with BlockUntilHistoryProcessesPendingRequests.

class QuittingHistoryDBTask : public history::HistoryDBTask {
 public:
  QuittingHistoryDBTask() {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

 private:
  virtual ~QuittingHistoryDBTask() {}

  DISALLOW_COPY_AND_ASSIGN(QuittingHistoryDBTask);
};

class TestExtensionURLRequestContext : public net::URLRequestContext {
 public:
  TestExtensionURLRequestContext() {
    net::CookieMonster* cookie_monster =
        content::CreateCookieStore(content::CookieStoreConfig())->
            GetCookieMonster();
    const char* const schemes[] = {extensions::kExtensionScheme};
    cookie_monster->SetCookieableSchemes(schemes, arraysize(schemes));
    set_cookie_store(cookie_monster);
  }

  virtual ~TestExtensionURLRequestContext() {
    AssertNoURLRequests();
  }
};

class TestExtensionURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
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

#if defined(ENABLE_NOTIFICATIONS)
KeyedService* CreateTestDesktopNotificationService(
    content::BrowserContext* profile) {
  return new DesktopNotificationService(static_cast<Profile*>(profile), NULL);
}
#endif

}  // namespace

// static
#if defined(OS_CHROMEOS)
// Must be kept in sync with
// ChromeBrowserMainPartsChromeos::PreEarlyInitialization.
const char TestingProfile::kTestUserProfileDir[] = "test-user";
#else
const char TestingProfile::kTestUserProfileDir[] = "Default";
#endif

TestingProfile::TestingProfile()
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      incognito_(false),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(NULL) {
  CreateTempProfileDir();
  profile_path_ = temp_dir_.path();

  Init();
  FinishInit();
}

TestingProfile::TestingProfile(const base::FilePath& path)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      incognito_(false),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(NULL) {
  Init();
  FinishInit();
}

TestingProfile::TestingProfile(const base::FilePath& path,
                               Delegate* delegate)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      incognito_(false),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(delegate) {
  Init();
  if (delegate_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestingProfile::FinishInit, base::Unretained(this)));
  } else {
    FinishInit();
  }
}

TestingProfile::TestingProfile(
    const base::FilePath& path,
    Delegate* delegate,
#if defined(ENABLE_EXTENSIONS)
    scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy,
#endif
    scoped_ptr<PrefServiceSyncable> prefs,
    bool incognito,
    bool guest_session,
    const std::string& supervised_user_id,
    scoped_ptr<policy::PolicyService> policy_service,
    const TestingFactories& factories)
    : start_time_(Time::Now()),
      prefs_(prefs.release()),
      testing_prefs_(NULL),
      incognito_(incognito),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(guest_session),
      supervised_user_id_(supervised_user_id),
      last_session_exited_cleanly_(true),
#if defined(ENABLE_EXTENSIONS)
      extension_special_storage_policy_(extension_policy),
#endif
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(delegate),
      policy_service_(policy_service.release()) {
  // If no profile path was supplied, create one.
  if (profile_path_.empty()) {
    CreateTempProfileDir();
    profile_path_ = temp_dir_.path();
  }

  // Set any testing factories prior to initializing the services.
  for (TestingFactories::const_iterator it = factories.begin();
       it != factories.end(); ++it) {
    it->first->SetTestingFactory(this, it->second);
  }

  Init();
  // If caller supplied a delegate, delay the FinishInit invocation until other
  // tasks have run.
  // TODO(atwilson): See if this is still required once we convert the current
  // users of the constructor that takes a Delegate* param.
  if (delegate_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestingProfile::FinishInit, base::Unretained(this)));
  } else {
    FinishInit();
  }
}

void TestingProfile::CreateTempProfileDir() {
  if (!temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create unique temporary directory.";

    // Fallback logic in case we fail to create unique temporary directory.
    base::FilePath system_tmp_dir;
    bool success = PathService::Get(base::DIR_TEMP, &system_tmp_dir);

    // We're severly screwed if we can't get the system temporary
    // directory. Die now to avoid writing to the filesystem root
    // or other bad places.
    CHECK(success);

    base::FilePath fallback_dir(
        system_tmp_dir.AppendASCII("TestingProfilePath"));
    base::DeleteFile(fallback_dir, true);
    base::CreateDirectory(fallback_dir);
    if (!temp_dir_.Set(fallback_dir)) {
      // That shouldn't happen, but if it does, try to recover.
      LOG(ERROR) << "Failed to use a fallback temporary directory.";

      // We're screwed if this fails, see CHECK above.
      CHECK(temp_dir_.Set(system_tmp_dir));
    }
  }
}

void TestingProfile::Init() {
  // If threads have been initialized, we should be on the UI thread.
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if defined(OS_ANDROID)
  // Make sure token service knows its running in tests.
  AndroidProfileOAuth2TokenService::set_is_testing_profile();
#endif

  // Normally this would happen during browser startup, but for tests
  // we need to trigger creation of Profile-related services.
  ChromeBrowserMainExtraPartsProfiles::
      EnsureBrowserContextKeyedServiceFactoriesBuilt();

  if (prefs_.get())
    user_prefs::UserPrefs::Set(this, prefs_.get());
  else
    CreateTestingPrefService();

  if (!base::PathExists(profile_path_))
    base::CreateDirectory(profile_path_);

  // TODO(joaodasilva): remove this once this PKS isn't created in ProfileImpl
  // anymore, after converting the PrefService to a PKS. Until then it must
  // be associated with a TestingProfile too.
  if (!IsOffTheRecord())
    CreateProfilePolicyConnector();

  extensions::ExtensionSystemFactory::GetInstance()->SetTestingFactory(
      this, extensions::TestExtensionSystem::Build);

  // If no original profile was specified for this profile: register preferences
  // even if this is an incognito profile - this allows tests to create a
  // standalone incognito profile while still having prefs registered.
  if (!IsOffTheRecord() || !original_profile_) {
    user_prefs::PrefRegistrySyncable* pref_registry =
        static_cast<user_prefs::PrefRegistrySyncable*>(
            prefs_->DeprecatedGetPrefRegistry());
    browser_context_dependency_manager_->
        RegisterProfilePrefsForServices(this, pref_registry);
  }

  browser_context_dependency_manager_->CreateBrowserContextServicesForTest(
      this);

#if defined(ENABLE_NOTIFICATIONS)
  // Install profile keyed service factory hooks for dummy/test services
  DesktopNotificationServiceFactory::GetInstance()->SetTestingFactory(
      this, CreateTestDesktopNotificationService);
#endif

#if defined(ENABLE_MANAGED_USERS)
  SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(this);
  TestingPrefStore* store = new TestingPrefStore();
  settings_service->Init(store);
  store->SetInitializationCompleted();
#endif

  profile_name_ = "testing_profile";
}

void TestingProfile::FinishInit() {
  DCHECK(content::NotificationService::current());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(static_cast<Profile*>(this)),
      content::NotificationService::NoDetails());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->InitProfileUserPrefs(this);

  if (delegate_)
    delegate_->OnProfileCreated(this, true, false);
}

TestingProfile::~TestingProfile() {
  // Revert to non-incognito mode before shutdown.
  force_incognito_ = false;

  // Any objects holding live URLFetchers should be deleted before teardown.
  TemplateURLFetcherFactory::ShutdownForProfile(this);

  MaybeSendDestroyedNotification();

  browser_context_dependency_manager_->DestroyBrowserContextServices(this);

  if (host_content_settings_map_.get())
    host_content_settings_map_->ShutdownOnUIThread();

  DestroyTopSites();

  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();
  // Failing a post == leaks == heapcheck failure. Make that an immediate test
  // failure.
  if (resource_context_) {
    CHECK(BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
                                    resource_context_));
    resource_context_ = NULL;
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }
}

static KeyedService* BuildFaviconService(content::BrowserContext* profile) {
  FaviconClient* favicon_client =
      ChromeFaviconClientFactory::GetForProfile(static_cast<Profile*>(profile));
  return new FaviconService(static_cast<Profile*>(profile), favicon_client);
}

void TestingProfile::CreateFaviconService() {
  // It is up to the caller to create the history service if one is needed.
  FaviconServiceFactory::GetInstance()->SetTestingFactory(
      this, BuildFaviconService);
}

static KeyedService* BuildHistoryService(content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new HistoryService(ChromeHistoryClientFactory::GetForProfile(profile),
                            profile);
}

bool TestingProfile::CreateHistoryService(bool delete_file, bool no_db) {
  DestroyHistoryService();
  if (delete_file) {
    base::FilePath path = GetPath();
    path = path.Append(chrome::kHistoryFilename);
    if (!base::DeleteFile(path, false) || base::PathExists(path))
      return false;
  }
  // This will create and init the history service.
  HistoryService* history_service = static_cast<HistoryService*>(
      HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          this, BuildHistoryService));
  if (!history_service->Init(this->GetPath(), no_db)) {
    HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(this, NULL);
  }
  // Disable WebHistoryService by default, since it makes network requests.
  WebHistoryServiceFactory::GetInstance()->SetTestingFactory(this, NULL);
  return true;
}

void TestingProfile::DestroyHistoryService() {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(this);
  if (!history_service)
    return;

  history_service->ClearCachedDataForContextID(0);
  history_service->SetOnBackendDestroyTask(base::MessageLoop::QuitClosure());
  history_service->Cleanup();
  HistoryServiceFactory::ShutdownForProfile(this);

  // Wait for the backend class to terminate before deleting the files and
  // moving to the next test. Note: if this never terminates, somebody is
  // probably leaking a reference to the history backend, so it never calls
  // our destroy task.
  base::MessageLoop::current()->Run();

  // Make sure we don't have any event pending that could disrupt the next
  // test.
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
  base::MessageLoop::current()->Run();
}

void TestingProfile::CreateTopSites() {
  DestroyTopSites();
  top_sites_ = history::TopSites::Create(
      this, GetPath().Append(chrome::kTopSitesFilename));
}

void TestingProfile::DestroyTopSites() {
  if (top_sites_.get()) {
    top_sites_->Shutdown();
    top_sites_ = NULL;
    // TopSitesImpl::Shutdown schedules some tasks (from TopSitesBackend) that
    // need to be run to properly shutdown. Run all pending tasks now. This is
    // normally handled by browser_process shutdown.
    if (base::MessageLoop::current())
      base::MessageLoop::current()->RunUntilIdle();
  }
}

static KeyedService* BuildBookmarkModel(content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  ChromeBookmarkClient* bookmark_client =
      ChromeBookmarkClientFactory::GetForProfile(profile);
  BookmarkModel* bookmark_model = new BookmarkModel(bookmark_client, false);
  bookmark_client->Init(bookmark_model);
  bookmark_model->Load(profile->GetPrefs(),
                       profile->GetPrefs()->GetString(prefs::kAcceptLanguages),
                       profile->GetPath(),
                       profile->GetIOTaskRunner(),
                       content::BrowserThread::GetMessageLoopProxyForThread(
                           content::BrowserThread::UI));
  return bookmark_model;
}

static KeyedService* BuildChromeBookmarkClient(
    content::BrowserContext* context) {
  return new ChromeBookmarkClient(static_cast<Profile*>(context));
}

static KeyedService* BuildChromeHistoryClient(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new ChromeHistoryClient(BookmarkModelFactory::GetForProfile(profile));
}

void TestingProfile::CreateBookmarkModel(bool delete_file) {
  if (delete_file) {
    base::FilePath path = GetPath().Append(bookmarks::kBookmarksFileName);
    base::DeleteFile(path, false);
  }
  ChromeHistoryClientFactory::GetInstance()->SetTestingFactory(
      this, BuildChromeHistoryClient);
  ChromeBookmarkClientFactory::GetInstance()->SetTestingFactory(
      this, BuildChromeBookmarkClient);
  // This creates the BookmarkModel.
  ignore_result(BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
      this, BuildBookmarkModel));
}

static KeyedService* BuildWebDataService(content::BrowserContext* profile) {
  return new WebDataServiceWrapper(static_cast<Profile*>(profile));
}

void TestingProfile::CreateWebDataService() {
  WebDataServiceFactory::GetInstance()->SetTestingFactory(
      this, BuildWebDataService);
}

void TestingProfile::BlockUntilHistoryIndexIsRefreshed() {
  // Only get the history service if it actually exists since the caller of the
  // test should explicitly call CreateHistoryService to build it.
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(this);
  DCHECK(history_service);
  history::InMemoryURLIndex* index = history_service->InMemoryIndex();
  if (!index || index->restored())
    return;
  base::RunLoop run_loop;
  HistoryIndexRestoreObserver observer(
      content::GetQuitTaskForRunLoop(&run_loop));
  index->set_restore_cache_observer(&observer);
  run_loop.Run();
  index->set_restore_cache_observer(NULL);
  DCHECK(index->restored());
}

// TODO(phajdan.jr): Doesn't this hang if Top Sites are already loaded?
void TestingProfile::BlockUntilTopSitesLoaded() {
  content::WindowedNotificationObserver top_sites_loaded_observer(
      chrome::NOTIFICATION_TOP_SITES_LOADED,
      content::NotificationService::AllSources());
  top_sites_loaded_observer.Wait();
}

void TestingProfile::SetGuestSession(bool guest) {
  guest_session_ = guest;
}

base::FilePath TestingProfile::GetPath() const {
  return profile_path_;
}

scoped_refptr<base::SequencedTaskRunner> TestingProfile::GetIOTaskRunner() {
  return base::MessageLoop::current()->message_loop_proxy();
}

TestingPrefServiceSyncable* TestingProfile::GetTestingPrefService() {
  if (!prefs_.get())
    CreateTestingPrefService();
  DCHECK(testing_prefs_);
  return testing_prefs_;
}

TestingProfile* TestingProfile::AsTestingProfile() {
  return this;
}

std::string TestingProfile::GetProfileName() {
  return profile_name_;
}

Profile::ProfileType TestingProfile::GetProfileType() const {
  if (guest_session_)
    return GUEST_PROFILE;
  if (force_incognito_ || incognito_)
    return INCOGNITO_PROFILE;
  return REGULAR_PROFILE;
}

bool TestingProfile::IsOffTheRecord() const {
  return force_incognito_ || incognito_;
}

void TestingProfile::SetOffTheRecordProfile(scoped_ptr<Profile> profile) {
  DCHECK(!IsOffTheRecord());
  incognito_profile_ = profile.Pass();
}

void TestingProfile::SetOriginalProfile(Profile* profile) {
  DCHECK(IsOffTheRecord());
  original_profile_ = profile;
}

Profile* TestingProfile::GetOffTheRecordProfile() {
  if (IsOffTheRecord())
    return this;
  if (!incognito_profile_) {
    TestingProfile::Builder builder;
    builder.SetIncognito();
    scoped_ptr<TestingProfile> incognito_test_profile(builder.Build());
    incognito_test_profile->SetOriginalProfile(this);
    SetOffTheRecordProfile(incognito_test_profile.PassAs<Profile>());
  }
  return incognito_profile_.get();
}

bool TestingProfile::HasOffTheRecordProfile() {
  return incognito_profile_.get() != NULL;
}

Profile* TestingProfile::GetOriginalProfile() {
  if (original_profile_)
    return original_profile_;
  return this;
}

bool TestingProfile::IsSupervised() {
  return !supervised_user_id_.empty();
}

#if defined(ENABLE_EXTENSIONS)
void TestingProfile::SetExtensionSpecialStoragePolicy(
    ExtensionSpecialStoragePolicy* extension_special_storage_policy) {
  extension_special_storage_policy_ = extension_special_storage_policy;
}
#endif

ExtensionSpecialStoragePolicy*
TestingProfile::GetExtensionSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  if (!extension_special_storage_policy_.get())
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy(NULL);
  return extension_special_storage_policy_.get();
#else
  return NULL;
#endif
}

net::CookieMonster* TestingProfile::GetCookieMonster() {
  if (!GetRequestContext())
    return NULL;
  return GetRequestContext()->GetURLRequestContext()->cookie_store()->
      GetCookieMonster();
}

void TestingProfile::CreateTestingPrefService() {
  DCHECK(!prefs_.get());
  testing_prefs_ = new TestingPrefServiceSyncable();
  prefs_.reset(testing_prefs_);
  user_prefs::UserPrefs::Set(this, prefs_.get());
  chrome::RegisterUserProfilePrefs(testing_prefs_->registry());
}

void TestingProfile::CreateProfilePolicyConnector() {
#if defined(ENABLE_CONFIGURATION_POLICY)
  schema_registry_service_ =
      policy::SchemaRegistryServiceFactory::CreateForContext(
          this, policy::Schema(), NULL);
  CHECK_EQ(schema_registry_service_.get(),
           policy::SchemaRegistryServiceFactory::GetForContext(this));
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

if (!policy_service_) {
#if defined(ENABLE_CONFIGURATION_POLICY)
    std::vector<policy::ConfigurationPolicyProvider*> providers;
    policy_service_.reset(new policy::PolicyServiceImpl(providers));
#else
    policy_service_.reset(new policy::PolicyServiceStub());
#endif
  }
  profile_policy_connector_.reset(new policy::ProfilePolicyConnector());
  profile_policy_connector_->InitForTesting(policy_service_.Pass());
  policy::ProfilePolicyConnectorFactory::GetInstance()->SetServiceForTesting(
      this, profile_policy_connector_.get());
  CHECK_EQ(profile_policy_connector_.get(),
           policy::ProfilePolicyConnectorFactory::GetForProfile(this));
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
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* TestingProfile::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return new net::TestURLRequestContextGetter(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  content::RenderProcessHost* rph = content::RenderProcessHost::FromID(
      renderer_child_id);
  return rph->GetStoragePartition()->GetURLRequestContext();
}

net::URLRequestContextGetter* TestingProfile::GetMediaRequestContext() {
  return NULL;
}

net::URLRequestContextGetter*
TestingProfile::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter*
TestingProfile::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return NULL;
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForExtensions() {
  if (!extensions_request_context_.get())
    extensions_request_context_ = new TestExtensionURLRequestContextGetter();
  return extensions_request_context_.get();
}

net::SSLConfigService* TestingProfile::GetSSLConfigService() {
  if (!GetRequestContext())
    return NULL;
  return GetRequestContext()->GetURLRequestContext()->ssl_config_service();
}

net::URLRequestContextGetter*
TestingProfile::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  // We don't test storage partitions here yet, so returning the same dummy
  // context is sufficient for now.
  return GetRequestContext();
}

content::ResourceContext* TestingProfile::GetResourceContext() {
  if (!resource_context_)
    resource_context_ = new content::MockResourceContext();
  return resource_context_;
}

HostContentSettingsMap* TestingProfile::GetHostContentSettingsMap() {
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(GetPrefs(), false);
#if defined(ENABLE_EXTENSIONS)
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(this)->extension_service();
    if (extension_service)
      host_content_settings_map_->RegisterExtensionService(extension_service);
#endif
  }
  return host_content_settings_map_.get();
}

content::BrowserPluginGuestManager* TestingProfile::GetGuestManager() {
#if defined(ENABLE_EXTENSIONS)
  return GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

content::PushMessagingService* TestingProfile::GetPushMessagingService() {
  return NULL;
}

bool TestingProfile::IsSameProfile(Profile *p) {
  return this == p;
}

base::Time TestingProfile::GetStartTime() const {
  return start_time_;
}

base::FilePath TestingProfile::last_selected_directory() {
  return last_selected_directory_;
}

void TestingProfile::set_last_selected_directory(const base::FilePath& path) {
  last_selected_directory_ = path;
}

PrefProxyConfigTracker* TestingProfile::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_.get()) {
    // TestingProfile is used in unit tests, where local state is not available.
    pref_proxy_config_tracker_.reset(
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(GetPrefs(),
                                                                   NULL));
  }
  return pref_proxy_config_tracker_.get();
}

void TestingProfile::BlockUntilHistoryProcessesPendingRequests() {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(this, Profile::EXPLICIT_ACCESS);
  DCHECK(history_service);
  DCHECK(base::MessageLoop::current());

  base::CancelableTaskTracker tracker;
  history_service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new QuittingHistoryDBTask()),
      &tracker);
  base::MessageLoop::current()->Run();
}

chrome_browser_net::Predictor* TestingProfile::GetNetworkPredictor() {
  return NULL;
}

DevToolsNetworkController* TestingProfile::GetDevToolsNetworkController() {
  return NULL;
}

void TestingProfile::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  if (!completion.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, completion);
  }
}

GURL TestingProfile::GetHomePage() {
  return GURL(chrome::kChromeUINewTabURL);
}

PrefService* TestingProfile::GetOffTheRecordPrefs() {
  return NULL;
}

quota::SpecialStoragePolicy* TestingProfile::GetSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

bool TestingProfile::WasCreatedByVersionOrLater(const std::string& version) {
  return true;
}

bool TestingProfile::IsGuestSession() const {
  return guest_session_;
}

Profile::ExitType TestingProfile::GetLastSessionExitType() {
  return last_session_exited_cleanly_ ? EXIT_NORMAL : EXIT_CRASHED;
}

TestingProfile::Builder::Builder()
    : build_called_(false),
      delegate_(NULL),
      incognito_(false),
      guest_session_(false) {
}

TestingProfile::Builder::~Builder() {
}

void TestingProfile::Builder::SetPath(const base::FilePath& path) {
  path_ = path;
}

void TestingProfile::Builder::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

#if defined(ENABLE_EXTENSIONS)
void TestingProfile::Builder::SetExtensionSpecialStoragePolicy(
    scoped_refptr<ExtensionSpecialStoragePolicy> policy) {
  extension_policy_ = policy;
}
#endif

void TestingProfile::Builder::SetPrefService(
    scoped_ptr<PrefServiceSyncable> prefs) {
  pref_service_ = prefs.Pass();
}

void TestingProfile::Builder::SetIncognito() {
  incognito_ = true;
}

void TestingProfile::Builder::SetGuestSession() {
  guest_session_ = true;
}

void TestingProfile::Builder::SetSupervisedUserId(
    const std::string& supervised_user_id) {
  supervised_user_id_ = supervised_user_id;
}

void TestingProfile::Builder::SetPolicyService(
    scoped_ptr<policy::PolicyService> policy_service) {
  policy_service_ = policy_service.Pass();
}

void TestingProfile::Builder::AddTestingFactory(
    BrowserContextKeyedServiceFactory* service_factory,
    BrowserContextKeyedServiceFactory::TestingFactoryFunction callback) {
  testing_factories_.push_back(std::make_pair(service_factory, callback));
}

scoped_ptr<TestingProfile> TestingProfile::Builder::Build() {
  DCHECK(!build_called_);
  build_called_ = true;

  return scoped_ptr<TestingProfile>(new TestingProfile(
      path_,
      delegate_,
#if defined(ENABLE_EXTENSIONS)
      extension_policy_,
#endif
      pref_service_.Pass(),
      incognito_,
      guest_session_,
      supervised_user_id_,
      policy_service_.Pass(),
      testing_factories_));
}

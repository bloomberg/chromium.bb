// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"

#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/visit_delegate.h"
#include "components/history/ios/browser/history_database_helper.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_client_impl.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_keyed_service_factories.h"
#include "ios/chrome/browser/history/history_client_impl.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"
#include "ios/chrome/browser/sync/glue/sync_start_util.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
std::unique_ptr<KeyedService> BuildHistoryService(web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return base::MakeUnique<history::HistoryService>(
      base::WrapUnique(new HistoryClientImpl(
          ios::BookmarkModelFactory::GetForBrowserState(browser_state))),
      nullptr);
}

std::unique_ptr<KeyedService> BuildBookmarkModel(web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model(
      new bookmarks::BookmarkModel(
          base::MakeUnique<BookmarkClientImpl>(browser_state)));
  bookmark_model->Load(
      browser_state->GetPrefs(),
      browser_state->GetStatePath(), browser_state->GetIOTaskRunner(),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::UI));
  // TODO(crbug.com/703565): remove std::move() once Xcode 9.0+ is required.
  return std::move(bookmark_model);
}

void NotReachedErrorCallback(WebDataServiceWrapper::ErrorType,
                             sql::InitStatus,
                             const std::string&) {
  NOTREACHED();
}

std::unique_ptr<KeyedService> BuildWebDataService(web::BrowserState* context) {
  const base::FilePath& browser_state_path = context->GetStatePath();
  return base::MakeUnique<WebDataServiceWrapper>(
      browser_state_path, GetApplicationContext()->GetApplicationLocale(),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::UI),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::DB),
      ios::sync_start_util::GetFlareForSyncableService(browser_state_path),
      &NotReachedErrorCallback);
}

base::FilePath CreateTempBrowserStateDir(base::ScopedTempDir* temp_dir) {
  DCHECK(temp_dir);
  if (!temp_dir->CreateUniqueTempDir()) {
    // Fallback logic in case we fail to create unique temporary directory.
    LOG(ERROR) << "Failed to create unique temporary directory.";
    base::FilePath system_tmp_dir;
    bool success = PathService::Get(base::DIR_TEMP, &system_tmp_dir);

    // We're severly screwed if we can't get the system temporary
    // directory. Die now to avoid writing to the filesystem root
    // or other bad places.
    CHECK(success);

    base::FilePath fallback_dir(
        system_tmp_dir.Append(FILE_PATH_LITERAL("TestChromeBrowserStatePath")));
    base::DeleteFile(fallback_dir, true);
    base::CreateDirectory(fallback_dir);
    if (!temp_dir->Set(fallback_dir)) {
      // That shouldn't happen, but if it does, try to recover.
      LOG(ERROR) << "Failed to use a fallback temporary directory.";

      // We're screwed if this fails, see CHECK above.
      CHECK(temp_dir->Set(system_tmp_dir));
    }
  }
  return temp_dir->GetPath();
}
}  // namespace

TestChromeBrowserState::TestChromeBrowserState(
    TestChromeBrowserState* original_browser_state)
    : testing_prefs_(nullptr),
      otr_browser_state_(nullptr),
      original_browser_state_(original_browser_state) {
  // Not calling Init() here as the bi-directional link between original and
  // off-the-record TestChromeBrowserState must be established before this
  // method can be called.
  DCHECK(original_browser_state_);
}

TestChromeBrowserState::TestChromeBrowserState(
    const base::FilePath& path,
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
    const TestingFactories& testing_factories,
    const RefcountedTestingFactories& refcounted_testing_factories)
    : state_path_(path),
      prefs_(std::move(prefs)),
      testing_prefs_(nullptr),
      otr_browser_state_(nullptr),
      original_browser_state_(nullptr) {
  Init();

  for (const auto& pair : testing_factories) {
    pair.first->SetTestingFactory(this, pair.second);
  }

  for (const auto& pair : refcounted_testing_factories) {
    pair.first->SetTestingFactory(this, pair.second);
  }
}

TestChromeBrowserState::~TestChromeBrowserState() {
  // If this TestChromeBrowserState owns an incognito TestChromeBrowserState,
  // tear it down first.
  otr_browser_state_.reset();

  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);
}

void TestChromeBrowserState::Init() {
  // If threads have been initialized, we should be on the UI thread.
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));

  if (state_path_.empty())
    state_path_ = CreateTempBrowserStateDir(&temp_dir_);

  if (IsOffTheRecord())
    state_path_ = state_path_.Append(FILE_PATH_LITERAL("OTR"));

  if (!base::PathExists(state_path_))
    base::CreateDirectory(state_path_);

  BrowserState::Initialize(this, GetStatePath());

  // Normally this would happen during browser startup, but for tests we need to
  // trigger creation of BrowserState-related services.
  EnsureBrowserStateKeyedServiceFactoriesBuilt();

  if (prefs_) {
    // If user passed a custom PrefServiceSyncable, then leave |testing_prefs_|
    // unset as it is not possible to determine its type.
  } else if (IsOffTheRecord()) {
    // This leaves |testing_prefs_| unset as CreateIncognitoBrowserStatePrefs()
    // does not return a TestingPrefServiceSyncable.
    DCHECK(original_browser_state_);
    prefs_ =
        CreateIncognitoBrowserStatePrefs(original_browser_state_->prefs_.get());
  } else {
    testing_prefs_ = new sync_preferences::TestingPrefServiceSyncable();
    RegisterBrowserStatePrefs(testing_prefs_->registry());
    prefs_.reset(testing_prefs_);
  }
  user_prefs::UserPrefs::Set(this, prefs_.get());

  // Prefs for incognito TestChromeBrowserState are set in
  // CreateIncognitoBrowserStatePrefs().
  if (!IsOffTheRecord()) {
    user_prefs::PrefRegistrySyncable* pref_registry =
        static_cast<user_prefs::PrefRegistrySyncable*>(
            prefs_->DeprecatedGetPrefRegistry());
    BrowserStateDependencyManager::GetInstance()
        ->RegisterBrowserStatePrefsForServices(this, pref_registry);
  }

  BrowserStateDependencyManager::GetInstance()
      ->CreateBrowserStateServicesForTest(this);
}

bool TestChromeBrowserState::IsOffTheRecord() const {
  return original_browser_state_ != nullptr;
}

base::FilePath TestChromeBrowserState::GetStatePath() const {
  if (!IsOffTheRecord())
    return state_path_;

  base::FilePath otr_stash_state_path =
      state_path_.Append(FILE_PATH_LITERAL("OTR"));
  if (!base::PathExists(otr_stash_state_path))
    base::CreateDirectory(otr_stash_state_path);
  return otr_stash_state_path;
}

scoped_refptr<base::SequencedTaskRunner>
TestChromeBrowserState::GetIOTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

ios::ChromeBrowserState*
TestChromeBrowserState::GetOriginalChromeBrowserState() {
  if (IsOffTheRecord())
    return original_browser_state_;
  return this;
}

bool TestChromeBrowserState::HasOffTheRecordChromeBrowserState() const {
  return otr_browser_state_ != nullptr;
}

ios::ChromeBrowserState*
TestChromeBrowserState::GetOffTheRecordChromeBrowserState() {
  if (IsOffTheRecord())
    return this;

  if (!otr_browser_state_) {
    otr_browser_state_.reset(new TestChromeBrowserState(this));
    otr_browser_state_->Init();
  }

  return otr_browser_state_.get();
}

PrefProxyConfigTracker* TestChromeBrowserState::GetProxyConfigTracker() {
  return nullptr;
}

net::SSLConfigService* TestChromeBrowserState::GetSSLConfigService() {
  return nullptr;
}

PrefService* TestChromeBrowserState::GetPrefs() {
  return prefs_.get();
}

PrefService* TestChromeBrowserState::GetOffTheRecordPrefs() {
  return nullptr;
}

ChromeBrowserStateIOData* TestChromeBrowserState::GetIOData() {
  return nullptr;
}

void TestChromeBrowserState::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  if (!completion.is_null())
    completion.Run();
}

net::URLRequestContextGetter* TestChromeBrowserState::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  return new net::TestURLRequestContextGetter(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO));
}

net::URLRequestContextGetter*
TestChromeBrowserState::CreateIsolatedRequestContext(
    const base::FilePath& partition_path) {
  return nullptr;
}

void TestChromeBrowserState::CreateWebDataService() {
  ignore_result(
      ios::WebDataServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          this, &BuildWebDataService));

  // Wait a bit after creating the WebDataService to allow the initialisation
  // to complete (otherwise the TestChromeBrowserState may be destroyed before
  // initialisation of the database is complete which leads to SQL init errors).
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void TestChromeBrowserState::CreateBookmarkModel(bool delete_file) {
  if (delete_file) {
    base::DeleteFile(GetOriginalChromeBrowserState()->GetStatePath().Append(
                         bookmarks::kBookmarksFileName),
                     false /* recursive */);
  }
  ignore_result(
      ios::BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
          this, &BuildBookmarkModel));
}

bool TestChromeBrowserState::CreateHistoryService(bool delete_file) {
  // Ensure that no HistoryService exists before creating a new one.
  DestroyHistoryService();

  if (delete_file) {
    base::FilePath path =
        GetOriginalChromeBrowserState()->GetStatePath().Append(
            history::kHistoryFilename);
    if (!base::DeleteFile(path, false) && base::PathExists(path))
      return false;
  }

  // Create and initialize the HistoryService, but destroy it if the init fails.
  history::HistoryService* history_service =
      static_cast<history::HistoryService*>(
          ios::HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              this, &BuildHistoryService));
  if (!history_service->Init(
          history::HistoryDatabaseParamsForPath(
              GetOriginalChromeBrowserState()->GetStatePath()))) {
    ios::HistoryServiceFactory::GetInstance()->SetTestingFactory(this, nullptr);
    return false;
  }

  // Some tests expect that CreateHistoryService() will also make the
  // InMemoryURLIndex available.
  ios::InMemoryURLIndexFactory::GetInstance()->SetTestingFactory(
      this, ios::InMemoryURLIndexFactory::GetDefaultFactory());
  // Disable WebHistoryService by default, since it makes network requests.
  ios::WebHistoryServiceFactory::GetInstance()->SetTestingFactory(this,
                                                                  nullptr);

  return true;
}

void TestChromeBrowserState::DestroyHistoryService() {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetInstance()->GetForBrowserStateIfExists(
          this, ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service)
    return;

  base::RunLoop run_loop;

  history_service->ClearCachedDataForContextID(0);
  history_service->SetOnBackendDestroyTask(run_loop.QuitWhenIdleClosure());
  history_service->Shutdown();
  history_service = nullptr;

  // Resetting the testing factory force the destruction of the current
  // HistoryService instance associated with the TestChromeBrowserState.
  ios::HistoryServiceFactory::GetInstance()->SetTestingFactory(this, nullptr);

  // Wait for the backend class to terminate before deleting the files and
  // moving to the next test. Note: if this never terminates, somebody is
  // probably leaking a reference to the history backend, so it never calls
  // our destroy task.
  run_loop.Run();
}

sync_preferences::TestingPrefServiceSyncable*
TestChromeBrowserState::GetTestingPrefService() {
  DCHECK(prefs_);
  DCHECK(testing_prefs_);
  return testing_prefs_;
}

TestChromeBrowserState::Builder::Builder() : build_called_(false) {}

TestChromeBrowserState::Builder::~Builder() {}

void TestChromeBrowserState::Builder::AddTestingFactory(
    BrowserStateKeyedServiceFactory* service_factory,
    BrowserStateKeyedServiceFactory::TestingFactoryFunction cb) {
  DCHECK(!build_called_);
  testing_factories_.push_back(std::make_pair(service_factory, cb));
}

void TestChromeBrowserState::Builder::AddTestingFactory(
    RefcountedBrowserStateKeyedServiceFactory* service_factory,
    RefcountedBrowserStateKeyedServiceFactory::TestingFactoryFunction cb) {
  DCHECK(!build_called_);
  refcounted_testing_factories_.push_back(std::make_pair(service_factory, cb));
}

void TestChromeBrowserState::Builder::SetPath(const base::FilePath& path) {
  DCHECK(!build_called_);
  state_path_ = path;
}

void TestChromeBrowserState::Builder::SetPrefService(
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs) {
  DCHECK(!build_called_);
  pref_service_ = std::move(prefs);
}

std::unique_ptr<TestChromeBrowserState>
TestChromeBrowserState::Builder::Build() {
  DCHECK(!build_called_);
  return base::WrapUnique(new TestChromeBrowserState(
      state_path_, std::move(pref_service_), testing_factories_,
      refcounted_testing_factories_));
}

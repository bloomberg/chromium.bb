// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state_impl_io_data.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store_impl.h"
#include "components/domain_reliability/monitor.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_constants.h"
#include "ios/chrome/browser/data_reduction_proxy/ios_chrome_data_reduction_proxy_io_data.h"
#include "ios/chrome/browser/data_reduction_proxy/ios_chrome_data_reduction_proxy_settings.h"
#include "ios/chrome/browser/data_reduction_proxy/ios_chrome_data_reduction_proxy_settings_factory.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/net/cookie_util.h"
#include "ios/chrome/browser/net/http_server_properties_manager_factory.h"
#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"
#include "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/public/web_thread.h"
#include "net/base/cache_type.h"
#include "net/base/sdch_manager.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/sdch/sdch_owner.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace {

// Connects the SdchOwner's storage to the prefs.
class SdchOwnerPrefStorage : public net::SdchOwner::PrefStorage,
                             public PrefStore::Observer {
 public:
  explicit SdchOwnerPrefStorage(PersistentPrefStore* storage)
      : storage_(storage), storage_key_("SDCH"), init_observer_(nullptr) {}
  ~SdchOwnerPrefStorage() override {
    if (init_observer_)
      storage_->RemoveObserver(this);
  }

  ReadError GetReadError() const override {
    PersistentPrefStore::PrefReadError error = storage_->GetReadError();

    DCHECK_NE(
        error,
        PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE);
    DCHECK_NE(error, PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM);

    switch (error) {
      case PersistentPrefStore::PREF_READ_ERROR_NONE:
        return PERSISTENCE_FAILURE_NONE;

      case PersistentPrefStore::PREF_READ_ERROR_NO_FILE:
        return PERSISTENCE_FAILURE_REASON_NO_FILE;

      case PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE:
      case PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE:
      case PersistentPrefStore::PREF_READ_ERROR_FILE_OTHER:
      case PersistentPrefStore::PREF_READ_ERROR_FILE_LOCKED:
      case PersistentPrefStore::PREF_READ_ERROR_JSON_REPEAT:
        return PERSISTENCE_FAILURE_REASON_READ_FAILED;

      case PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED:
      case PersistentPrefStore::PREF_READ_ERROR_FILE_NOT_SPECIFIED:
      case PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE:
      case PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM:
      default:
        // We don't expect these other failures given our usage of prefs.
        NOTREACHED();
        return PERSISTENCE_FAILURE_REASON_OTHER;
    }
  }

  bool GetValue(const base::DictionaryValue** result) const override {
    const base::Value* result_value = nullptr;
    if (!storage_->GetValue(storage_key_, &result_value))
      return false;
    return result_value->GetAsDictionary(result);
  }

  bool GetMutableValue(base::DictionaryValue** result) override {
    base::Value* result_value = nullptr;
    if (!storage_->GetMutableValue(storage_key_, &result_value))
      return false;
    return result_value->GetAsDictionary(result);
  }

  void SetValue(scoped_ptr<base::DictionaryValue> value) override {
    storage_->SetValue(storage_key_, std::move(value),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }

  void ReportValueChanged() override {
    storage_->ReportValueChanged(storage_key_,
                                 WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }

  bool IsInitializationComplete() override {
    return storage_->IsInitializationComplete();
  }

  void StartObservingInit(net::SdchOwner* observer) override {
    DCHECK(!init_observer_);
    init_observer_ = observer;
    storage_->AddObserver(this);
  }

  void StopObservingInit() override {
    DCHECK(init_observer_);
    init_observer_ = nullptr;
    storage_->RemoveObserver(this);
  }

 private:
  // PrefStore::Observer implementation.
  void OnPrefValueChanged(const std::string& key) override {}
  void OnInitializationCompleted(bool succeeded) override {
    init_observer_->OnPrefStorageInitializationComplete(succeeded);
  }

  PersistentPrefStore* storage_;  // Non-owning.
  const std::string storage_key_;

  net::SdchOwner* init_observer_;  // Non-owning.

  DISALLOW_COPY_AND_ASSIGN(SdchOwnerPrefStorage);
};

}  // namespace

ChromeBrowserStateImplIOData::Handle::Handle(
    ios::ChromeBrowserState* browser_state)
    : io_data_(new ChromeBrowserStateImplIOData),
      browser_state_(browser_state),
      initialized_(false) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  DCHECK(browser_state);
}

ChromeBrowserStateImplIOData::Handle::~Handle() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  if (io_data_->http_server_properties_manager_)
    io_data_->http_server_properties_manager_->ShutdownOnPrefThread();

  // io_data_->data_reduction_proxy_io_data() might be NULL if Init() was
  // never called.
  if (io_data_->data_reduction_proxy_io_data())
    io_data_->data_reduction_proxy_io_data()->ShutdownOnUIThread();

  io_data_->ShutdownOnUIThread(GetAllContextGetters());
}

void ChromeBrowserStateImplIOData::Handle::Init(
    const base::FilePath& cookie_path,
    const base::FilePath& channel_id_path,
    const base::FilePath& cache_path,
    int cache_max_size,
    const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  DCHECK(!io_data_->lazy_params_);

  LazyParams* lazy_params = new LazyParams();

  lazy_params->cookie_path = cookie_path;
  lazy_params->channel_id_path = channel_id_path;
  lazy_params->cache_path = cache_path;
  lazy_params->cache_max_size = cache_max_size;
  io_data_->lazy_params_.reset(lazy_params);

  // Keep track of profile path and cache sizes separately so we can use them
  // on demand when creating storage isolated URLRequestContextGetters.
  io_data_->profile_path_ = profile_path;
  io_data_->app_cache_max_size_ = cache_max_size;

  io_data_->InitializeMetricsEnabledStateOnUIThread();

  // TODO(tbansal): Move this to IO thread once the data reduction proxy
  // params are unified into a single object.
  bool enable_quic_for_data_reduction_proxy =
      IOSChromeIOThread::ShouldEnableQuicForDataReductionProxy();

  io_data_->set_data_reduction_proxy_io_data(
      CreateIOSChromeDataReductionProxyIOData(
          GetApplicationContext()->GetIOSChromeIOThread()->net_log(),
          browser_state_->GetPrefs(),
          web::WebThread::GetTaskRunnerForThread(web::WebThread::IO),
          web::WebThread::GetTaskRunnerForThread(web::WebThread::UI),
          enable_quic_for_data_reduction_proxy));

  base::SequencedWorkerPool* pool = web::WebThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      pool->GetSequencedTaskRunnerWithShutdownBehavior(
          pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  scoped_ptr<data_reduction_proxy::DataStore> store(
      new data_reduction_proxy::DataStoreImpl(profile_path));
  IOSChromeDataReductionProxySettingsFactory::GetForBrowserState(browser_state_)
      ->InitDataReductionProxySettings(
          io_data_->data_reduction_proxy_io_data(), browser_state_->GetPrefs(),
          browser_state_->GetRequestContext(), std::move(store),
          web::WebThread::GetTaskRunnerForThread(web::WebThread::UI),
          db_task_runner);
}

scoped_refptr<IOSChromeURLRequestContextGetter>
ChromeBrowserStateImplIOData::Handle::CreateMainRequestContextGetter(
    ProtocolHandlerMap* protocol_handlers,
    PrefService* local_state,
    IOSChromeIOThread* io_thread) const {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  LazyInitialize();
  DCHECK(!main_request_context_getter_.get());
  main_request_context_getter_ =
      IOSChromeURLRequestContextGetter::Create(io_data_, protocol_handlers);

  return main_request_context_getter_;
}

scoped_refptr<IOSChromeURLRequestContextGetter>
ChromeBrowserStateImplIOData::Handle::CreateIsolatedAppRequestContextGetter(
    const base::FilePath& partition_path) const {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  // Check that the partition_path is not the same as the base profile path. We
  // expect isolated partition, which will never go to the default profile path.
  CHECK(partition_path != browser_state_->GetStatePath());
  LazyInitialize();

  // Keep a map of request context getters, one per requested storage partition.
  IOSChromeURLRequestContextGetterMap::iterator iter =
      app_request_context_getter_map_.find(partition_path);
  if (iter != app_request_context_getter_map_.end())
    return iter->second;

  IOSChromeURLRequestContextGetter* context =
      IOSChromeURLRequestContextGetter::CreateForIsolatedApp(
          browser_state_->GetRequestContext(), io_data_, partition_path);
  app_request_context_getter_map_[partition_path] = context;
  return context;
}

ChromeBrowserStateIOData* ChromeBrowserStateImplIOData::Handle::io_data()
    const {
  LazyInitialize();
  return io_data_;
}

void ChromeBrowserStateImplIOData::Handle::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  LazyInitialize();

  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(
          &ChromeBrowserStateImplIOData::ClearNetworkingHistorySinceOnIOThread,
          base::Unretained(io_data_), time, completion));
}

void ChromeBrowserStateImplIOData::Handle::LazyInitialize() const {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  PrefService* pref_service = browser_state_->GetPrefs();
  io_data_->http_server_properties_manager_ =
      HttpServerPropertiesManagerFactory::CreateManager(pref_service);
  io_data_->set_http_server_properties(scoped_ptr<net::HttpServerProperties>(
      io_data_->http_server_properties_manager_));
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
                                          pref_service);
  io_data_->safe_browsing_enabled()->MoveToThread(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO));
  io_data_->InitializeOnUIThread(browser_state_);
}

scoped_ptr<ChromeBrowserStateIOData::IOSChromeURLRequestContextGetterVector>
ChromeBrowserStateImplIOData::Handle::GetAllContextGetters() {
  IOSChromeURLRequestContextGetterMap::iterator iter;
  scoped_ptr<IOSChromeURLRequestContextGetterVector> context_getters(
      new IOSChromeURLRequestContextGetterVector());

  iter = app_request_context_getter_map_.begin();
  for (; iter != app_request_context_getter_map_.end(); ++iter)
    context_getters->push_back(iter->second);

  if (main_request_context_getter_.get())
    context_getters->push_back(main_request_context_getter_);

  return context_getters;
}

ChromeBrowserStateImplIOData::LazyParams::LazyParams() : cache_max_size(0) {}

ChromeBrowserStateImplIOData::LazyParams::~LazyParams() {}

ChromeBrowserStateImplIOData::ChromeBrowserStateImplIOData()
    : ChromeBrowserStateIOData(
          ios::ChromeBrowserStateType::REGULAR_BROWSER_STATE),
      http_server_properties_manager_(nullptr),
      app_cache_max_size_(0) {}

ChromeBrowserStateImplIOData::~ChromeBrowserStateImplIOData() {}

void ChromeBrowserStateImplIOData::InitializeInternal(
    scoped_ptr<IOSChromeNetworkDelegate> chrome_network_delegate,
    ProfileParams* profile_params,
    ProtocolHandlerMap* protocol_handlers) const {
  // Set up a persistent store for use by the network stack on the IO thread.
  base::FilePath network_json_store_filepath(
      profile_path_.Append(kIOSChromeNetworkPersistentStateFilename));
  network_json_store_ = new JsonPrefStore(
      network_json_store_filepath,
      JsonPrefStore::GetTaskRunnerForFile(network_json_store_filepath,
                                          web::WebThread::GetBlockingPool()),
      scoped_ptr<PrefFilter>());
  network_json_store_->ReadPrefsAsync(nullptr);

  net::URLRequestContext* main_context = main_request_context();

  IOSChromeIOThread* const io_thread = profile_params->io_thread;
  IOSChromeIOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(main_context);

  if (http_server_properties_manager_)
    http_server_properties_manager_->InitializeOnNetworkThread();

  main_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());

  network_delegate_ = data_reduction_proxy_io_data()->CreateNetworkDelegate(
      std::move(chrome_network_delegate), true);

  main_context->set_network_delegate(network_delegate_.get());

  main_context->set_http_server_properties(http_server_properties());

  main_context->set_host_resolver(io_thread_globals->host_resolver.get());

  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  main_context->set_proxy_service(proxy_service());
  main_context->set_backoff_manager(
      io_thread_globals->url_request_backoff_manager.get());

  scoped_refptr<net::CookieStore> cookie_store = NULL;
  net::ChannelIDService* channel_id_service = NULL;

  // Set up cookie store.
  if (!cookie_store.get()) {
    DCHECK(!lazy_params_->cookie_path.empty());
    cookie_util::CookieStoreConfig ios_cookie_config(
        lazy_params_->cookie_path,
        cookie_util::CookieStoreConfig::RESTORED_SESSION_COOKIES,
        cookie_util::CookieStoreConfig::COOKIE_STORE_IOS,
        cookie_config::GetCookieCryptoDelegate());
    cookie_store = cookie_util::CreateCookieStore(ios_cookie_config);

    if (profile_params->path.BaseName().value() ==
        kIOSChromeInitialBrowserState) {
      // Enable metrics on the default profile, not secondary profiles.
      static_cast<net::CookieStoreIOS*>(cookie_store.get())
          ->SetMetricsEnabled();
    }
  }

  main_context->set_cookie_store(cookie_store.get());

  // Set up server bound cert service.
  if (!channel_id_service) {
    DCHECK(!lazy_params_->channel_id_path.empty());

    scoped_refptr<net::SQLiteChannelIDStore> channel_id_db =
        new net::SQLiteChannelIDStore(
            lazy_params_->channel_id_path,
            web::WebThread::GetBlockingPool()->GetSequencedTaskRunner(
                web::WebThread::GetBlockingPool()->GetSequenceToken()));
    channel_id_service = new net::ChannelIDService(
        new net::DefaultChannelIDStore(channel_id_db.get()),
        base::WorkerPool::GetTaskRunner(true));
  }

  set_channel_id_service(channel_id_service);
  main_context->set_channel_id_service(channel_id_service);

  scoped_ptr<net::HttpCache::BackendFactory> main_backend(
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE, net::CACHE_BACKEND_BLOCKFILE,
          lazy_params_->cache_path, lazy_params_->cache_max_size,
          web::WebThread::GetTaskRunnerForThread(web::WebThread::CACHE)));
  http_network_session_ = CreateHttpNetworkSession(*profile_params);
  main_http_factory_ = CreateMainHttpFactory(http_network_session_.get(),
                                             std::move(main_backend));
  main_context->set_http_transaction_factory(main_http_factory_.get());

  scoped_ptr<net::URLRequestJobFactoryImpl> main_job_factory(
      new net::URLRequestJobFactoryImpl());
  InstallProtocolHandlers(main_job_factory.get(), protocol_handlers);

  // The data reduction proxy interceptor should be as close to the network as
  // possible.
  URLRequestInterceptorScopedVector request_interceptors;
  request_interceptors.insert(
      request_interceptors.begin(),
      data_reduction_proxy_io_data()->CreateInterceptor().release());
  main_job_factory_ = SetUpJobFactoryDefaults(std::move(main_job_factory),
                                              std::move(request_interceptors),
                                              main_context->network_delegate());
  main_context->set_job_factory(main_job_factory_.get());
  main_context->set_network_quality_estimator(
      io_thread_globals->network_quality_estimator.get());

  // Setup SDCH for this profile.
  sdch_manager_.reset(new net::SdchManager);
  sdch_policy_.reset(new net::SdchOwner(sdch_manager_.get(), main_context));
  main_context->set_sdch_manager(sdch_manager_.get());
  sdch_policy_->EnablePersistentStorage(
      make_scoped_ptr(new SdchOwnerPrefStorage(network_json_store_.get())));

  lazy_params_.reset();
}

net::URLRequestContext*
ChromeBrowserStateImplIOData::InitializeAppRequestContext(
    net::URLRequestContext* main_context) const {
  // Copy most state from the main context.
  AppRequestContext* context = new AppRequestContext();
  context->CopyFrom(main_context);

  // Use a separate HTTP disk cache for isolated apps.
  scoped_ptr<net::HttpCache::BackendFactory> app_backend =
      net::HttpCache::DefaultBackend::InMemory(0);
  scoped_ptr<net::HttpCache> app_http_cache =
      CreateHttpFactory(http_network_session_.get(), std::move(app_backend));

  cookie_util::CookieStoreConfig ios_cookie_config(
      base::FilePath(),
      cookie_util::CookieStoreConfig::EPHEMERAL_SESSION_COOKIES,
      cookie_util::CookieStoreConfig::COOKIE_STORE_IOS, nullptr);
  scoped_refptr<net::CookieStore> cookie_store =
      cookie_util::CreateCookieStore(ios_cookie_config);

  // Transfer ownership of the cookies and cache to AppRequestContext.
  context->SetCookieStore(cookie_store.get());
  context->SetHttpTransactionFactory(std::move(app_http_cache));

  scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  // The data reduction proxy interceptor should be as close to the network as
  // possible.
  URLRequestInterceptorScopedVector request_interceptors;
  request_interceptors.insert(
      request_interceptors.begin(),
      data_reduction_proxy_io_data()->CreateInterceptor().release());
  scoped_ptr<net::URLRequestJobFactory> top_job_factory(SetUpJobFactoryDefaults(
      std::move(job_factory), std::move(request_interceptors),
      main_context->network_delegate()));
  context->SetJobFactory(std::move(top_job_factory));

  return context;
}

net::URLRequestContext*
ChromeBrowserStateImplIOData::AcquireIsolatedAppRequestContext(
    net::URLRequestContext* main_context) const {
  // We create per-app contexts on demand, unlike the others above.
  net::URLRequestContext* app_request_context =
      InitializeAppRequestContext(main_context);
  DCHECK(app_request_context);
  return app_request_context;
}

void ChromeBrowserStateImplIOData::ClearNetworkingHistorySinceOnIOThread(
    base::Time time,
    const base::Closure& completion) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  DCHECK(initialized());

  DCHECK(transport_security_state());
  // Completes synchronously.
  transport_security_state()->DeleteAllDynamicDataSince(time);
  DCHECK(http_server_properties_manager_);
  http_server_properties_manager_->Clear(completion);
}

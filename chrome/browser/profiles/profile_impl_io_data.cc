// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl_io_data.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/http_server_properties_manager_factory.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/quota_policy_channel_id_store.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_io_data.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store_impl.h"
#include "components/domain_reliability/monitor.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_io_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/common/constants.h"
#include "extensions/features/features.h"
#include "net/base/cache_type.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/reporting/reporting_feature.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/quota/special_storage_policy.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/offline_pages/offline_page_request_interceptor.h"
#endif  // defined(OS_ANDROID)

namespace {

net::BackendType ChooseCacheBackendType() {
#if !defined(OS_ANDROID)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUseSimpleCacheBackend)) {
    const std::string opt_value =
        command_line.GetSwitchValueASCII(switches::kUseSimpleCacheBackend);
    if (base::LowerCaseEqualsASCII(opt_value, "off"))
      return net::CACHE_BACKEND_BLOCKFILE;
    if (opt_value.empty() || base::LowerCaseEqualsASCII(opt_value, "on"))
      return net::CACHE_BACKEND_SIMPLE;
  }
  const std::string experiment_name =
      base::FieldTrialList::FindFullName("SimpleCacheTrial");
  if (base::StartsWith(experiment_name, "Disable",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return net::CACHE_BACKEND_BLOCKFILE;
  }
  if (base::StartsWith(experiment_name, "ExperimentYes",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return net::CACHE_BACKEND_SIMPLE;
  }
#endif  // #if !defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  return net::CACHE_BACKEND_SIMPLE;
#else
  return net::CACHE_BACKEND_BLOCKFILE;
#endif
}

}  // namespace

using content::BrowserThread;

ProfileImplIOData::Handle::Handle(Profile* profile)
    : io_data_(new ProfileImplIOData),
      profile_(profile),
      initialized_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile);
}

ProfileImplIOData::Handle::~Handle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (io_data_->predictor_ != NULL) {
    // io_data_->predictor_ might be NULL if Init() was never called
    // (i.e. we shut down before ProfileImpl::DoFinalInit() got called).
    io_data_->predictor_->ShutdownOnUIThread();
  }

  if (io_data_->http_server_properties_manager_)
    io_data_->http_server_properties_manager_->ShutdownOnPrefThread();

  // io_data_->data_reduction_proxy_io_data() might be NULL if Init() was
  // never called.
  if (io_data_->data_reduction_proxy_io_data())
    io_data_->data_reduction_proxy_io_data()->ShutdownOnUIThread();

  io_data_->ShutdownOnUIThread(GetAllContextGetters());
}

void ProfileImplIOData::Handle::Init(
    const base::FilePath& cookie_path,
    const base::FilePath& channel_id_path,
    const base::FilePath& cache_path,
    int cache_max_size,
    const base::FilePath& media_cache_path,
    int media_cache_max_size,
    const base::FilePath& extensions_cookie_path,
    const base::FilePath& profile_path,
    chrome_browser_net::Predictor* predictor,
    content::CookieStoreConfig::SessionCookieMode session_cookie_mode,
    storage::SpecialStoragePolicy* special_storage_policy,
    std::unique_ptr<domain_reliability::DomainReliabilityMonitor>
        domain_reliability_monitor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!io_data_->lazy_params_);
  DCHECK(predictor);

  LazyParams* lazy_params = new LazyParams();

  lazy_params->cookie_path = cookie_path;
  lazy_params->channel_id_path = channel_id_path;
  lazy_params->cache_path = cache_path;
  lazy_params->cache_max_size = cache_max_size;
  lazy_params->media_cache_path = media_cache_path;
  lazy_params->media_cache_max_size = media_cache_max_size;
  lazy_params->extensions_cookie_path = extensions_cookie_path;
  lazy_params->session_cookie_mode = session_cookie_mode;
  lazy_params->special_storage_policy = special_storage_policy;
  lazy_params->domain_reliability_monitor =
      std::move(domain_reliability_monitor);

  PrefService* pref_service = profile_->GetPrefs();
  lazy_params->http_server_properties_manager.reset(
      chrome_browser_net::HttpServerPropertiesManagerFactory::CreateManager(
          pref_service));
  io_data_->http_server_properties_manager_ =
      lazy_params->http_server_properties_manager.get();

  io_data_->lazy_params_.reset(lazy_params);

  // Keep track of profile path and cache sizes separately so we can use them
  // on demand when creating storage isolated URLRequestContextGetters.
  io_data_->profile_path_ = profile_path;
  io_data_->app_cache_max_size_ = cache_max_size;
  io_data_->app_media_cache_max_size_ = media_cache_max_size;

  io_data_->predictor_.reset(predictor);

  io_data_->InitializeMetricsEnabledStateOnUIThread();
  if (io_data_->lazy_params_->domain_reliability_monitor)
    io_data_->lazy_params_->domain_reliability_monitor->MoveToNetworkThread();

  io_data_->set_previews_io_data(base::MakeUnique<previews::PreviewsIOData>(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)));
  PreviewsServiceFactory::GetForProfile(profile_)->Initialize(
      io_data_->previews_io_data(),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO), profile_path);

  io_data_->set_data_reduction_proxy_io_data(
      CreateDataReductionProxyChromeIOData(
          g_browser_process->io_thread()->net_log(), profile_->GetPrefs(),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::UI)));
}

content::ResourceContext*
    ProfileImplIOData::Handle::GetResourceContext() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize();
  return GetResourceContextNoInit();
}

content::ResourceContext*
ProfileImplIOData::Handle::GetResourceContextNoInit() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Don't call LazyInitialize here, since the resource context is created at
  // the beginning of initalization and is used by some members while they're
  // being initialized (i.e. AppCacheService).
  return io_data_->GetResourceContext();
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::CreateMainRequestContextGetter(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors,
    IOThread* io_thread) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize();
  DCHECK(!main_request_context_getter_.get());
  main_request_context_getter_ = ChromeURLRequestContextGetter::Create(
      profile_, io_data_, protocol_handlers, std::move(request_interceptors));

  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  std::unique_ptr<data_reduction_proxy::DataStore> store(
      new data_reduction_proxy::DataStoreImpl(io_data_->profile_path_));
  DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_)
      ->InitDataReductionProxySettings(
          io_data_->data_reduction_proxy_io_data(), profile_->GetPrefs(),
          main_request_context_getter_.get(), std::move(store),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
          db_task_runner);

  io_data_->predictor_
      ->InitNetworkPredictor(profile_->GetPrefs(),
                             io_thread,
                             main_request_context_getter_.get(),
                             io_data_);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
  return main_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetMediaRequestContextGetter() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize();
  if (!media_request_context_getter_.get()) {
    media_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateForMedia(profile_, io_data_);
  }
  return media_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetExtensionsRequestContextGetter() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize();
  if (!extensions_request_context_getter_.get()) {
    extensions_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateForExtensions(profile_, io_data_);
  }
  return extensions_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::CreateIsolatedAppRequestContextGetter(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Check that the partition_path is not the same as the base profile path. We
  // expect isolated partition, which will never go to the default profile path.
  CHECK(partition_path != profile_->GetPath());
  LazyInitialize();

  // Keep a map of request context getters, one per requested storage partition.
  StoragePartitionDescriptor descriptor(partition_path, in_memory);
  ChromeURLRequestContextGetterMap::iterator iter =
      app_request_context_getter_map_.find(descriptor);
  if (iter != app_request_context_getter_map_.end())
    return iter->second;

  std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
      protocol_handler_interceptor(
          ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_)
              ->CreateJobInterceptorFactory());
  ChromeURLRequestContextGetter* context =
      ChromeURLRequestContextGetter::CreateForIsolatedApp(
          profile_, io_data_, descriptor,
          std::move(protocol_handler_interceptor), protocol_handlers,
          std::move(request_interceptors));
  app_request_context_getter_map_[descriptor] = context;

  return context;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetIsolatedMediaRequestContextGetter(
    const base::FilePath& partition_path,
    bool in_memory) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We must have a non-default path, or this will act like the default media
  // context.
  CHECK(partition_path != profile_->GetPath());
  LazyInitialize();

  // Keep a map of request context getters, one per requested storage partition.
  StoragePartitionDescriptor descriptor(partition_path, in_memory);
  ChromeURLRequestContextGetterMap::iterator iter =
      isolated_media_request_context_getter_map_.find(descriptor);
  if (iter != isolated_media_request_context_getter_map_.end())
    return iter->second;

  // Get the app context as the starting point for the media context, so that
  // it uses the app's cookie store.
  ChromeURLRequestContextGetterMap::const_iterator app_iter =
      app_request_context_getter_map_.find(descriptor);
  DCHECK(app_iter != app_request_context_getter_map_.end());
  ChromeURLRequestContextGetter* app_context = app_iter->second.get();
  ChromeURLRequestContextGetter* context =
      ChromeURLRequestContextGetter::CreateForIsolatedMedia(
          profile_, app_context, io_data_, descriptor);
  isolated_media_request_context_getter_map_[descriptor] = context;

  return context;
}

DevToolsNetworkControllerHandle*
ProfileImplIOData::Handle::GetDevToolsNetworkControllerHandle() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return io_data_->network_controller_handle();
}

void ProfileImplIOData::Handle::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ProfileImplIOData::ClearNetworkingHistorySinceOnIOThread,
                     base::Unretained(io_data_), time, completion));
}

void ProfileImplIOData::Handle::LazyInitialize() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  PrefService* pref_service = profile_->GetPrefs();
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
      pref_service);
  io_data_->safe_browsing_enabled()->MoveToThread(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  io_data_->InitializeOnUIThread(profile_);
}

std::unique_ptr<ProfileIOData::ChromeURLRequestContextGetterVector>
ProfileImplIOData::Handle::GetAllContextGetters() {
  ChromeURLRequestContextGetterMap::iterator iter;
  std::unique_ptr<ChromeURLRequestContextGetterVector> context_getters(
      new ChromeURLRequestContextGetterVector());

  iter = isolated_media_request_context_getter_map_.begin();
  for (; iter != isolated_media_request_context_getter_map_.end(); ++iter)
    context_getters->push_back(iter->second);

  iter = app_request_context_getter_map_.begin();
  for (; iter != app_request_context_getter_map_.end(); ++iter)
    context_getters->push_back(iter->second);

  if (extensions_request_context_getter_.get())
    context_getters->push_back(extensions_request_context_getter_);

  if (media_request_context_getter_.get())
    context_getters->push_back(media_request_context_getter_);

  if (main_request_context_getter_.get())
    context_getters->push_back(main_request_context_getter_);

  return context_getters;
}

ProfileImplIOData::LazyParams::LazyParams()
    : cache_max_size(0),
      media_cache_max_size(0),
      session_cookie_mode(
          content::CookieStoreConfig::EPHEMERAL_SESSION_COOKIES) {}

ProfileImplIOData::LazyParams::~LazyParams() {}

ProfileImplIOData::ProfileImplIOData()
    : ProfileIOData(Profile::REGULAR_PROFILE),
      http_server_properties_manager_(NULL),
      domain_reliability_monitor_(nullptr),
      app_cache_max_size_(0),
      app_media_cache_max_size_(0) {
}

ProfileImplIOData::~ProfileImplIOData() {
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->Shutdown();

  DestroyResourceContext();

  if (media_request_context_)
    media_request_context_->AssertNoURLRequests();
}

std::unique_ptr<net::NetworkDelegate>
ProfileImplIOData::ConfigureNetworkDelegate(
    IOThread* io_thread,
    std::unique_ptr<ChromeNetworkDelegate> chrome_network_delegate) const {
  if (lazy_params_->domain_reliability_monitor) {
    // Hold on to a raw pointer to call Shutdown() in ~ProfileImplIOData.
    domain_reliability_monitor_ =
        lazy_params_->domain_reliability_monitor.get();

    domain_reliability_monitor_->InitURLRequestContext(main_request_context());
    domain_reliability_monitor_->AddBakedInConfigs();
    domain_reliability_monitor_->SetDiscardUploads(
        !GetMetricsEnabledStateOnIOThread());

    chrome_network_delegate->set_domain_reliability_monitor(
        std::move(lazy_params_->domain_reliability_monitor));
  }

  return data_reduction_proxy_io_data()->CreateNetworkDelegate(
      io_thread->globals()->data_use_ascriber->CreateNetworkDelegate(
          std::move(chrome_network_delegate),
          io_thread->GetMetricsDataUseForwarder()),
      true);
}

void ProfileImplIOData::InitializeInternal(
    ProfileParams* profile_params,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  net::URLRequestContext* main_context = main_request_context();
  net::URLRequestContextStorage* main_context_storage =
      main_request_context_storage();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(main_context);

  if (lazy_params_->http_server_properties_manager) {
    lazy_params_->http_server_properties_manager->InitializeOnNetworkThread();
    main_context_storage->set_http_server_properties(
        std::move(lazy_params_->http_server_properties_manager));
  }

  main_context->set_transport_security_state(transport_security_state());
  main_context->set_ct_policy_enforcer(
      io_thread_globals->ct_policy_enforcer.get());

  main_context->set_net_log(io_thread->net_log());

  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  main_context->set_proxy_service(proxy_service());

  // Create a single task runner to use with the CookieStore and ChannelIDStore.
  scoped_refptr<base::SequencedTaskRunner> cookie_background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  // Set up server bound cert service.
  DCHECK(!lazy_params_->channel_id_path.empty());
  scoped_refptr<QuotaPolicyChannelIDStore> channel_id_db =
      new QuotaPolicyChannelIDStore(lazy_params_->channel_id_path,
                                    cookie_background_task_runner,
                                    lazy_params_->special_storage_policy.get());
  main_context_storage->set_channel_id_service(
      base::MakeUnique<net::ChannelIDService>(
          new net::DefaultChannelIDStore(channel_id_db.get())));

  // Set up cookie store.
  DCHECK(!lazy_params_->cookie_path.empty());

  content::CookieStoreConfig cookie_config(
      lazy_params_->cookie_path, lazy_params_->session_cookie_mode,
      lazy_params_->special_storage_policy.get(),
      profile_params->cookie_monster_delegate.get());
  cookie_config.crypto_delegate = cookie_config::GetCookieCryptoDelegate();
  cookie_config.channel_id_service = main_context->channel_id_service();
  cookie_config.background_task_runner = cookie_background_task_runner;
  main_context_storage->set_cookie_store(
      content::CreateCookieStore(cookie_config));

  main_context->cookie_store()->SetChannelIDServiceID(
      main_context->channel_id_service()->GetUniqueID());

  std::unique_ptr<net::HttpCache::BackendFactory> main_backend(
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE, ChooseCacheBackendType(), lazy_params_->cache_path,
          lazy_params_->cache_max_size,
          BrowserThread::GetTaskRunnerForThread(BrowserThread::CACHE)));
  main_context_storage->set_http_network_session(
      CreateHttpNetworkSession(*profile_params));
  main_context_storage->set_http_transaction_factory(CreateMainHttpFactory(
      main_context_storage->http_network_session(), std::move(main_backend)));

  std::unique_ptr<net::URLRequestJobFactoryImpl> main_job_factory(
      new net::URLRequestJobFactoryImpl());
  InstallProtocolHandlers(main_job_factory.get(), protocol_handlers);

  // Install the Offline Page Interceptor.
#if defined(OS_ANDROID)
  request_interceptors.push_back(
      base::MakeUnique<offline_pages::OfflinePageRequestInterceptor>(
          previews_io_data()));
#endif

  // The data reduction proxy interceptor should be as close to the network
  // as possible.
  request_interceptors.insert(
      request_interceptors.begin(),
      data_reduction_proxy_io_data()->CreateInterceptor());
  main_context_storage->set_job_factory(SetUpJobFactoryDefaults(
      std::move(main_job_factory), std::move(request_interceptors),
      std::move(profile_params->protocol_handler_interceptor),
      main_context->network_delegate(),
      io_thread_globals->host_resolver.get()));
  main_context->set_network_quality_estimator(
      io_thread_globals->network_quality_estimator.get());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  InitializeExtensionsRequestContext(profile_params);
#endif

  main_context_storage->set_reporting_service(
      MaybeCreateReportingService(main_context));

  // Create a media request context based on the main context, but using a
  // media cache.  It shares the same job factory as the main context.
  StoragePartitionDescriptor details(profile_path_, false);
  media_request_context_.reset(
      InitializeMediaRequestContext(main_context, details, "main_media"));
  lazy_params_.reset();
}

void ProfileImplIOData::
    InitializeExtensionsRequestContext(ProfileParams* profile_params) const {
  // The extensions context only serves to hold onto the extensions cookie
  // store.
  net::URLRequestContext* extensions_context = extensions_request_context();

  content::CookieStoreConfig cookie_config(
      lazy_params_->extensions_cookie_path,
      lazy_params_->session_cookie_mode,
      NULL, NULL);
  cookie_config.crypto_delegate = cookie_config::GetCookieCryptoDelegate();
  // Enable cookies for chrome-extension URLs.
  cookie_config.cookieable_schemes.push_back(extensions::kExtensionScheme);
  cookie_config.channel_id_service = extensions_context->channel_id_service();
  extensions_cookie_store_ = content::CreateCookieStore(cookie_config);
  extensions_context->set_cookie_store(extensions_cookie_store_.get());
}

net::URLRequestContext* ProfileImplIOData::InitializeAppRequestContext(
    net::URLRequestContext* main_context,
    const StoragePartitionDescriptor& partition_descriptor,
    std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  // Copy most state from the main context.
  AppRequestContext* context = new AppRequestContext();
  context->CopyFrom(main_context);

  base::FilePath cookie_path = partition_descriptor.path.Append(
      chrome::kCookieFilename);
  base::FilePath channel_id_path =
      partition_descriptor.path.Append(chrome::kChannelIDFilename);
  base::FilePath cache_path =
      partition_descriptor.path.Append(chrome::kCacheDirname);

  // Use a separate HTTP disk cache for isolated apps.
  std::unique_ptr<net::HttpCache::BackendFactory> app_backend;
  if (partition_descriptor.in_memory) {
    app_backend = net::HttpCache::DefaultBackend::InMemory(0);
  } else {
    app_backend.reset(new net::HttpCache::DefaultBackend(
        net::DISK_CACHE, ChooseCacheBackendType(), cache_path,
        app_cache_max_size_,
        BrowserThread::GetTaskRunnerForThread(BrowserThread::CACHE)));
  }

  std::unique_ptr<net::CookieStore> cookie_store;
  scoped_refptr<net::SQLiteChannelIDStore> channel_id_db;
  // Create a single task runner to use with the CookieStore and ChannelIDStore.
  scoped_refptr<base::SequencedTaskRunner> cookie_background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  if (partition_descriptor.in_memory) {
    cookie_path = base::FilePath();
  }
  content::CookieStoreConfig cookie_config(
      cookie_path, content::CookieStoreConfig::EPHEMERAL_SESSION_COOKIES,
      nullptr, nullptr);
  if (!partition_descriptor.in_memory) {
    // Use an app-specific cookie store.
    DCHECK(!cookie_path.empty());

    // TODO(creis): We should have a cookie delegate for notifying the cookie
    // extensions API, but we need to update it to understand isolated apps
    // first.
    cookie_config.crypto_delegate = cookie_config::GetCookieCryptoDelegate();
    channel_id_db = new net::SQLiteChannelIDStore(
        channel_id_path, cookie_background_task_runner);
  }
  std::unique_ptr<net::ChannelIDService> channel_id_service(
      new net::ChannelIDService(
          new net::DefaultChannelIDStore(channel_id_db.get())));
  cookie_config.channel_id_service = channel_id_service.get();
  cookie_config.background_task_runner = cookie_background_task_runner;
  cookie_store = content::CreateCookieStore(cookie_config);
  cookie_store->SetChannelIDServiceID(channel_id_service->GetUniqueID());

  // Build a new HttpNetworkSession that uses the new ChannelIDService.
  // TODO(mmenke):  It weird to combine state from
  // main_request_context_storage() objects and the argumet to this method,
  // |main_context|.  Remove |main_context| as an argument, and just use
  // main_context() instead.
  net::HttpNetworkSession::Params network_params =
      main_request_context_storage()->http_network_session()->params();
  network_params.channel_id_service = channel_id_service.get();
  std::unique_ptr<net::HttpNetworkSession> http_network_session(
      new net::HttpNetworkSession(network_params));
  std::unique_ptr<net::HttpCache> app_http_cache =
      CreateMainHttpFactory(http_network_session.get(), std::move(app_backend));

  // Transfer ownership of the ChannelIDStore and the HttpNetworkSession to the
  // AppRequestContext.
  context->SetChannelIDService(std::move(channel_id_service));
  context->SetHttpNetworkSession(std::move(http_network_session));

  // Transfer ownership of the cookies and cache to AppRequestContext.
  context->SetCookieStore(std::move(cookie_store));
  context->SetHttpTransactionFactory(std::move(app_http_cache));

  std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  InstallProtocolHandlers(job_factory.get(), protocol_handlers);
  // The data reduction proxy interceptor should be as close to the network
  // as possible.
  request_interceptors.insert(
      request_interceptors.begin(),
      data_reduction_proxy_io_data()->CreateInterceptor());

  std::unique_ptr<net::URLRequestJobFactory> top_job_factory(
      SetUpJobFactoryDefaults(
          std::move(job_factory), std::move(request_interceptors),
          std::move(protocol_handler_interceptor), context->network_delegate(),
          context->host_resolver()));
  context->SetJobFactory(std::move(top_job_factory));

  context->SetReportingService(MaybeCreateReportingService(context));

  return context;
}

net::URLRequestContext* ProfileImplIOData::InitializeMediaRequestContext(
    net::URLRequestContext* original_context,
    const StoragePartitionDescriptor& partition_descriptor,
    const char* name) const {
  // Copy most state from the original context.
  MediaRequestContext* context = new MediaRequestContext(name);
  context->CopyFrom(original_context);

  // For in-memory context, return immediately after creating the new
  // context before attaching a separate cache. It is important to return
  // a new context rather than just reusing |original_context| because
  // the caller expects to take ownership of the pointer.
  if (partition_descriptor.in_memory)
    return context;

  using content::StoragePartition;
  base::FilePath cache_path;
  int cache_max_size = app_media_cache_max_size_;
  if (partition_descriptor.path == profile_path_) {
    // lazy_params_ is only valid for the default media context creation.
    cache_path = lazy_params_->media_cache_path;
    cache_max_size = lazy_params_->media_cache_max_size;
  } else {
    cache_path = partition_descriptor.path.Append(chrome::kMediaCacheDirname);
  }

  // Use a separate HTTP disk cache for isolated apps.
  std::unique_ptr<net::HttpCache::BackendFactory> media_backend(
      new net::HttpCache::DefaultBackend(
          net::MEDIA_CACHE, ChooseCacheBackendType(), cache_path,
          cache_max_size,
          BrowserThread::GetTaskRunnerForThread(BrowserThread::CACHE)));
  std::unique_ptr<net::HttpCache> media_http_cache = CreateHttpFactory(
      main_request_context()->http_transaction_factory(),
      std::move(media_backend));

  // Transfer ownership of the cache to MediaRequestContext.
  context->SetHttpTransactionFactory(std::move(media_http_cache));

  // Note that we do not create a new URLRequestJobFactory because
  // the media context should behave exactly like its parent context
  // in all respects except for cache behavior on media subresources.
  // The CopyFrom() step above means that our media context will use
  // the same URLRequestJobFactory instance that our parent context does.

  return context;
}

net::URLRequestContext*
ProfileImplIOData::AcquireMediaRequestContext() const {
  DCHECK(media_request_context_);
  return media_request_context_.get();
}

net::URLRequestContext* ProfileImplIOData::AcquireIsolatedAppRequestContext(
    net::URLRequestContext* main_context,
    const StoragePartitionDescriptor& partition_descriptor,
    std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  // We create per-app contexts on demand, unlike the others above.
  net::URLRequestContext* app_request_context = InitializeAppRequestContext(
      main_context, partition_descriptor,
      std::move(protocol_handler_interceptor), protocol_handlers,
      std::move(request_interceptors));
  DCHECK(app_request_context);
  return app_request_context;
}

net::URLRequestContext*
ProfileImplIOData::AcquireIsolatedMediaRequestContext(
    net::URLRequestContext* app_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  // We create per-app media contexts on demand, unlike the others above.
  net::URLRequestContext* media_request_context = InitializeMediaRequestContext(
      app_context, partition_descriptor, "isolated_media");
  DCHECK(media_request_context);
  return media_request_context;
}

chrome_browser_net::Predictor* ProfileImplIOData::GetPredictor() {
  return predictor_.get();
}

std::unique_ptr<net::ReportingService>
ProfileImplIOData::MaybeCreateReportingService(
    net::URLRequestContext* url_request_context) const {
  if (!base::FeatureList::IsEnabled(features::kReporting))
    return std::unique_ptr<net::ReportingService>();

  return net::ReportingService::Create(net::ReportingPolicy(),
                                       url_request_context);
}

void ProfileImplIOData::ClearNetworkingHistorySinceOnIOThread(
    base::Time time,
    const base::Closure& completion) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized());

  DCHECK(transport_security_state());
  // Completes synchronously.
  transport_security_state()->DeleteAllDynamicDataSince(time);
  DCHECK(http_server_properties_manager_);
  http_server_properties_manager_->Clear(completion);
}

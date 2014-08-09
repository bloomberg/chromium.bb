// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/about_protocol_handler.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/chrome_url_request_context_getter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/resource_context.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "net/base/sdch_dictionary_fetcher.h"
#include "net/base/sdch_manager.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "webkit/browser/database/database_tracker.h"

using content::BrowserThread;

OffTheRecordProfileIOData::Handle::Handle(Profile* profile)
    : io_data_(new OffTheRecordProfileIOData(profile->GetProfileType())),
      profile_(profile),
      initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
}

OffTheRecordProfileIOData::Handle::~Handle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ChromeURLRequestContextGetterMap::iterator iter =
      app_request_context_getter_map_.begin();
  for (; iter != app_request_context_getter_map_.end(); ++iter)
    iter->second->Invalidate();

  if (extensions_request_context_getter_)
    extensions_request_context_getter_->Invalidate();

  if (main_request_context_getter_)
    main_request_context_getter_->Invalidate();

  io_data_->ShutdownOnUIThread();
}

content::ResourceContext*
OffTheRecordProfileIOData::Handle::GetResourceContext() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  return GetResourceContextNoInit();
}

content::ResourceContext*
OffTheRecordProfileIOData::Handle::GetResourceContextNoInit() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Don't call LazyInitialize here, since the resource context is created at
  // the beginning of initalization and is used by some members while they're
  // being initialized (i.e. AppCacheService).
  return io_data_->GetResourceContext();
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::CreateMainRequestContextGetter(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  // TODO(oshima): Re-enable when ChromeOS only accesses the profile on the UI
  // thread.
#if !defined(OS_CHROMEOS)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#endif  // defined(OS_CHROMEOS)
  LazyInitialize();
  DCHECK(!main_request_context_getter_.get());
  main_request_context_getter_ = ChromeURLRequestContextGetter::Create(
      profile_, io_data_, protocol_handlers, request_interceptors.Pass());
  return main_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::GetExtensionsRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  if (!extensions_request_context_getter_.get()) {
    extensions_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateForExtensions(profile_, io_data_);
  }
  return extensions_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::GetIsolatedAppRequestContextGetter(
    const base::FilePath& partition_path,
    bool in_memory) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!partition_path.empty());
  LazyInitialize();

  // Keep a map of request context getters, one per requested app ID.
  StoragePartitionDescriptor descriptor(partition_path, in_memory);
  ChromeURLRequestContextGetterMap::iterator iter =
      app_request_context_getter_map_.find(descriptor);
  CHECK(iter != app_request_context_getter_map_.end());
  return iter->second;
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::CreateIsolatedAppRequestContextGetter(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!partition_path.empty());
  LazyInitialize();

  // Keep a map of request context getters, one per requested app ID.
  StoragePartitionDescriptor descriptor(partition_path, in_memory);
  DCHECK_EQ(app_request_context_getter_map_.count(descriptor), 0u);

  scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
      protocol_handler_interceptor(
          ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_)->
              CreateJobInterceptorFactory());
  ChromeURLRequestContextGetter* context =
      ChromeURLRequestContextGetter::CreateForIsolatedApp(
          profile_,
          io_data_,
          descriptor,
          protocol_handler_interceptor.Pass(),
          protocol_handlers,
          request_interceptors.Pass());
  app_request_context_getter_map_[descriptor] = context;

  return context;
}

DevToolsNetworkController*
OffTheRecordProfileIOData::Handle::GetDevToolsNetworkController() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return io_data_->network_controller();
}

void OffTheRecordProfileIOData::Handle::LazyInitialize() const {
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
      profile_->GetPrefs());
  io_data_->safe_browsing_enabled()->MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
#endif
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  io_data_->data_reduction_proxy_enabled()->Init(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled,
      profile_->GetPrefs());
  io_data_->data_reduction_proxy_enabled()->MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
#endif
  io_data_->InitializeOnUIThread(profile_);
}

OffTheRecordProfileIOData::OffTheRecordProfileIOData(
    Profile::ProfileType profile_type)
    : ProfileIOData(profile_type) {}

OffTheRecordProfileIOData::~OffTheRecordProfileIOData() {
  DestroyResourceContext();
}

void OffTheRecordProfileIOData::InitializeInternal(
    ProfileParams* profile_params,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  net::URLRequestContext* main_context = main_request_context();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(main_context);

  main_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());

  main_context->set_network_delegate(network_delegate());

  main_context->set_host_resolver(
      io_thread_globals->host_resolver.get());
  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());
  main_context->set_fraudulent_certificate_reporter(
      fraudulent_certificate_reporter());
  main_context->set_proxy_service(proxy_service());

  main_context->set_throttler_manager(
      io_thread_globals->throttler_manager.get());

  main_context->set_cert_transparency_verifier(
      io_thread_globals->cert_transparency_verifier.get());

  // For incognito, we use the default non-persistent HttpServerPropertiesImpl.
  set_http_server_properties(
      scoped_ptr<net::HttpServerProperties>(
          new net::HttpServerPropertiesImpl()));
  main_context->set_http_server_properties(http_server_properties());

  // For incognito, we use a non-persistent channel ID store.
  net::ChannelIDService* channel_id_service =
      new net::ChannelIDService(
          new net::DefaultChannelIDStore(NULL),
          base::WorkerPool::GetTaskRunner(true));
  set_channel_id_service(channel_id_service);
  main_context->set_channel_id_service(channel_id_service);

  using content::CookieStoreConfig;
  main_context->set_cookie_store(
      CreateCookieStore(CookieStoreConfig(
          base::FilePath(),
          CookieStoreConfig::EPHEMERAL_SESSION_COOKIES,
          NULL,
          profile_params->cookie_monster_delegate.get())));

  net::HttpCache::BackendFactory* main_backend =
      net::HttpCache::DefaultBackend::InMemory(0);
  main_http_factory_ = CreateMainHttpFactory(profile_params, main_backend);

  main_context->set_http_transaction_factory(main_http_factory_.get());
#if !defined(DISABLE_FTP_SUPPORT)
  ftp_factory_.reset(
      new net::FtpNetworkLayer(main_context->host_resolver()));
#endif  // !defined(DISABLE_FTP_SUPPORT)

  scoped_ptr<net::URLRequestJobFactoryImpl> main_job_factory(
      new net::URLRequestJobFactoryImpl());

  InstallProtocolHandlers(main_job_factory.get(), protocol_handlers);
  main_job_factory_ = SetUpJobFactoryDefaults(
      main_job_factory.Pass(),
      request_interceptors.Pass(),
      profile_params->protocol_handler_interceptor.Pass(),
      network_delegate(),
      ftp_factory_.get());
  main_context->set_job_factory(main_job_factory_.get());

  // Setup the SDCHManager for this profile.
  sdch_manager_.reset(new net::SdchManager);
  sdch_manager_->set_sdch_fetcher(
      new net::SdchDictionaryFetcher(
          sdch_manager_.get(),
          // SdchDictionaryFetcher takes a reference to the Getter, and
          // hence implicitly takes ownership.
          new net::TrivialURLRequestContextGetter(
              main_context,
              content::BrowserThread::GetMessageLoopProxyForThread(
                  content::BrowserThread::IO))));
  main_context->set_sdch_manager(sdch_manager_.get());

#if defined(ENABLE_EXTENSIONS)
  InitializeExtensionsRequestContext(profile_params);
#endif
}

void OffTheRecordProfileIOData::
    InitializeExtensionsRequestContext(ProfileParams* profile_params) const {
  net::URLRequestContext* extensions_context = extensions_request_context();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(extensions_context);

  extensions_context->set_transport_security_state(transport_security_state());

  extensions_context->set_net_log(io_thread->net_log());

  extensions_context->set_throttler_manager(
      io_thread_globals->throttler_manager.get());

  extensions_context->set_cert_transparency_verifier(
      io_thread_globals->cert_transparency_verifier.get());

  // All we care about for extensions is the cookie store. For incognito, we
  // use a non-persistent cookie store.
  net::CookieMonster* extensions_cookie_store =
      content::CreateCookieStore(content::CookieStoreConfig())->
          GetCookieMonster();
  // Enable cookies for devtools and extension URLs.
  const char* const schemes[] = {
      content::kChromeDevToolsScheme,
      extensions::kExtensionScheme
  };
  extensions_cookie_store->SetCookieableSchemes(schemes, arraysize(schemes));
  extensions_context->set_cookie_store(extensions_cookie_store);

  scoped_ptr<net::URLRequestJobFactoryImpl> extensions_job_factory(
      new net::URLRequestJobFactoryImpl());
  // TODO(shalev): The extensions_job_factory has a NULL NetworkDelegate.
  // Without a network_delegate, this protocol handler will never
  // handle file: requests, but as a side effect it makes
  // job_factory::IsHandledProtocol return true, which prevents attempts to
  // handle the protocol externally. We pass NULL in to
  // SetUpJobFactoryDefaults() to get this effect.
  extensions_job_factory_ = SetUpJobFactoryDefaults(
      extensions_job_factory.Pass(),
      content::URLRequestInterceptorScopedVector(),
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>(),
      NULL,
      ftp_factory_.get());
  extensions_context->set_job_factory(extensions_job_factory_.get());
}

net::URLRequestContext* OffTheRecordProfileIOData::InitializeAppRequestContext(
    net::URLRequestContext* main_context,
    const StoragePartitionDescriptor& partition_descriptor,
    scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  AppRequestContext* context = new AppRequestContext();

  // Copy most state from the main context.
  context->CopyFrom(main_context);

  // Use a separate in-memory cookie store for the app.
  // TODO(creis): We should have a cookie delegate for notifying the cookie
  // extensions API, but we need to update it to understand isolated apps first.
  context->SetCookieStore(
      content::CreateCookieStore(content::CookieStoreConfig()));

  // Use a separate in-memory cache for the app.
  net::HttpCache::BackendFactory* app_backend =
      net::HttpCache::DefaultBackend::InMemory(0);
  net::HttpNetworkSession* main_network_session =
      main_http_factory_->GetSession();
  scoped_ptr<net::HttpCache> app_http_cache =
      CreateHttpFactory(main_network_session, app_backend);

  context->SetHttpTransactionFactory(
      app_http_cache.PassAs<net::HttpTransactionFactory>());

  scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  InstallProtocolHandlers(job_factory.get(), protocol_handlers);
  scoped_ptr<net::URLRequestJobFactory> top_job_factory;
  top_job_factory = SetUpJobFactoryDefaults(job_factory.Pass(),
                                            request_interceptors.Pass(),
                                            protocol_handler_interceptor.Pass(),
                                            network_delegate(),
                                            ftp_factory_.get());
  context->SetJobFactory(top_job_factory.Pass());
  return context;
}

net::URLRequestContext*
OffTheRecordProfileIOData::InitializeMediaRequestContext(
    net::URLRequestContext* original_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  NOTREACHED();
  return NULL;
}

net::URLRequestContext*
OffTheRecordProfileIOData::AcquireMediaRequestContext() const {
  NOTREACHED();
  return NULL;
}

net::URLRequestContext*
OffTheRecordProfileIOData::AcquireIsolatedAppRequestContext(
    net::URLRequestContext* main_context,
    const StoragePartitionDescriptor& partition_descriptor,
    scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  // We create per-app contexts on demand, unlike the others above.
  net::URLRequestContext* app_request_context =
      InitializeAppRequestContext(main_context,
                                  partition_descriptor,
                                  protocol_handler_interceptor.Pass(),
                                  protocol_handlers,
                                  request_interceptors.Pass());
  DCHECK(app_request_context);
  return app_request_context;
}

net::URLRequestContext*
OffTheRecordProfileIOData::AcquireIsolatedMediaRequestContext(
    net::URLRequestContext* app_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  NOTREACHED();
  return NULL;
}

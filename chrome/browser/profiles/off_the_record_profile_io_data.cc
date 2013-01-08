// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
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
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "extensions/common/constants.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/server_bound_cert_service.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "webkit/database/database_tracker.h"

using content::BrowserThread;

OffTheRecordProfileIOData::Handle::Handle(Profile* profile)
    : io_data_(new OffTheRecordProfileIOData),
      profile_(profile),
      initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
}

OffTheRecordProfileIOData::Handle::~Handle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  io_data_->ShutdownOnUIThread();
}

base::Callback<ChromeURLDataManagerBackend*(void)>
OffTheRecordProfileIOData::Handle::
GetChromeURLDataManagerBackendGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  return base::Bind(&ProfileIOData::GetChromeURLDataManagerBackend,
                    base::Unretained(io_data_));
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
OffTheRecordProfileIOData::Handle::GetMainRequestContextGetter() const {
  // TODO(oshima): Re-enable when ChromeOS only accesses the profile on the UI
  // thread.
#if !defined(OS_CHROMEOS)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#endif  // defined(OS_CHROMEOS)
  LazyInitialize();
  if (!main_request_context_getter_) {
    main_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOffTheRecord(profile_, io_data_);
  }
  return main_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::GetExtensionsRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  if (!extensions_request_context_getter_) {
    extensions_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOffTheRecordForExtensions(
            profile_, io_data_);
  }
  return extensions_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::GetIsolatedAppRequestContextGetter(
    const FilePath& partition_path,
    bool in_memory) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!partition_path.empty());
  LazyInitialize();

  // Keep a map of request context getters, one per requested app ID.
  StoragePartitionDescriptor descriptor(partition_path, in_memory);
  ChromeURLRequestContextGetterMap::iterator iter =
      app_request_context_getter_map_.find(descriptor);
  if (iter != app_request_context_getter_map_.end())
    return iter->second;

  scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
      protocol_handler_interceptor(
          ProtocolHandlerRegistryFactory::GetForProfile(profile_)->
              CreateJobInterceptorFactory());
  ChromeURLRequestContextGetter* context =
      ChromeURLRequestContextGetter::CreateOffTheRecordForIsolatedApp(
          profile_, io_data_, descriptor,
          protocol_handler_interceptor.Pass());
  app_request_context_getter_map_[descriptor] = context;

  return context;
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
  io_data_->InitializeOnUIThread(profile_);
}

OffTheRecordProfileIOData::OffTheRecordProfileIOData()
    : ProfileIOData(true) {}
OffTheRecordProfileIOData::~OffTheRecordProfileIOData() {
  DestroyResourceContext();
}

void OffTheRecordProfileIOData::LazyInitializeInternal(
    ProfileParams* profile_params) const {
  ChromeURLRequestContext* main_context = main_request_context();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(main_context);

  main_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());

  main_context->set_network_delegate(network_delegate());

  main_context->set_host_resolver(
      io_thread_globals->host_resolver.get());
  main_context->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());
  main_context->set_fraudulent_certificate_reporter(
      fraudulent_certificate_reporter());
  main_context->set_proxy_service(proxy_service());

  main_context->set_throttler_manager(
      io_thread_globals->throttler_manager.get());

  // For incognito, we use the default non-persistent HttpServerPropertiesImpl.
  set_http_server_properties(new net::HttpServerPropertiesImpl);
  main_context->set_http_server_properties(http_server_properties());

  // For incognito, we use a non-persistent server bound cert store.
  net::ServerBoundCertService* server_bound_cert_service =
      new net::ServerBoundCertService(
          new net::DefaultServerBoundCertStore(NULL),
          base::WorkerPool::GetTaskRunner(true));
  set_server_bound_cert_service(server_bound_cert_service);
  main_context->set_server_bound_cert_service(server_bound_cert_service);

  main_context->set_cookie_store(
      new net::CookieMonster(NULL, profile_params->cookie_monster_delegate));

  net::HttpCache::BackendFactory* main_backend =
      net::HttpCache::DefaultBackend::InMemory(0);
  net::HttpNetworkSession::Params network_session_params;
  PopulateNetworkSessionParams(profile_params, &network_session_params);
  net::HttpCache* cache = new net::HttpCache(
      network_session_params, main_backend);

  main_http_factory_.reset(cache);
  main_context->set_http_transaction_factory(cache);
#if !defined(DISABLE_FTP_SUPPORT)
  ftp_factory_.reset(
      new net::FtpNetworkLayer(main_context->host_resolver()));
  main_context->set_ftp_transaction_factory(ftp_factory_.get());
#endif  // !defined(DISABLE_FTP_SUPPORT)

  main_context->set_chrome_url_data_manager_backend(
      chrome_url_data_manager_backend());

  scoped_ptr<net::URLRequestJobFactoryImpl> main_job_factory(
      new net::URLRequestJobFactoryImpl());

  main_job_factory_ = SetUpJobFactoryDefaults(
      main_job_factory.Pass(),
      profile_params->protocol_handler_interceptor.Pass(),
      network_delegate(),
      main_context->ftp_transaction_factory(),
      main_context->ftp_auth_cache());
  main_context->set_job_factory(main_job_factory_.get());

#if defined(ENABLE_EXTENSIONS)
  InitializeExtensionsRequestContext(profile_params);
#endif
}

void OffTheRecordProfileIOData::
    InitializeExtensionsRequestContext(ProfileParams* profile_params) const {
  ChromeURLRequestContext* extensions_context = extensions_request_context();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(extensions_context);

  extensions_context->set_transport_security_state(transport_security_state());

  extensions_context->set_net_log(io_thread->net_log());

  extensions_context->set_throttler_manager(
      io_thread_globals->throttler_manager.get());

  // All we care about for extensions is the cookie store. For incognito, we
  // use a non-persistent cookie store.
  net::CookieMonster* extensions_cookie_store =
      new net::CookieMonster(NULL, NULL);
  // Enable cookies for devtools and extension URLs.
  const char* schemes[] = {chrome::kChromeDevToolsScheme,
                           extensions::kExtensionScheme};
  extensions_cookie_store->SetCookieableSchemes(schemes, 2);
  extensions_context->set_cookie_store(extensions_cookie_store);

#if !defined(DISABLE_FTP_SUPPORT)
  DCHECK(ftp_factory_.get());
  extensions_context->set_ftp_transaction_factory(ftp_factory_.get());
#endif  // !defined(DISABLE_FTP_SUPPORT)

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
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>(NULL),
      NULL,
      extensions_context->ftp_transaction_factory(),
      extensions_context->ftp_auth_cache());
  extensions_context->set_job_factory(extensions_job_factory_.get());
}

ChromeURLRequestContext*
OffTheRecordProfileIOData::InitializeAppRequestContext(
    ChromeURLRequestContext* main_context,
    const StoragePartitionDescriptor& partition_descriptor,
    scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor) const {
  AppRequestContext* context = new AppRequestContext(load_time_stats());

  // Copy most state from the main context.
  context->CopyFrom(main_context);

  // Use a separate in-memory cookie store for the app.
  // TODO(creis): We should have a cookie delegate for notifying the cookie
  // extensions API, but we need to update it to understand isolated apps first.
  context->SetCookieStore(new net::CookieMonster(NULL, NULL));

  // Use a separate in-memory cache for the app.
  net::HttpCache::BackendFactory* app_backend =
      net::HttpCache::DefaultBackend::InMemory(0);
  net::HttpNetworkSession* main_network_session =
      main_http_factory_->GetSession();
  scoped_ptr<net::HttpTransactionFactory> app_http_cache(
      new net::HttpCache(main_network_session, app_backend));

  context->SetHttpTransactionFactory(app_http_cache.Pass());

  scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  scoped_ptr<net::URLRequestJobFactory> top_job_factory;
  top_job_factory = SetUpJobFactoryDefaults(job_factory.Pass(),
                                            protocol_handler_interceptor.Pass(),
                                            network_delegate(),
                                            context->ftp_transaction_factory(),
                                            context->ftp_auth_cache());
  context->SetJobFactory(top_job_factory.Pass());
  return context;
}

ChromeURLRequestContext*
OffTheRecordProfileIOData::InitializeMediaRequestContext(
    ChromeURLRequestContext* original_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  NOTREACHED();
  return NULL;
}

ChromeURLRequestContext*
OffTheRecordProfileIOData::AcquireMediaRequestContext() const {
  NOTREACHED();
  return NULL;
}

ChromeURLRequestContext*
OffTheRecordProfileIOData::AcquireIsolatedAppRequestContext(
    ChromeURLRequestContext* main_context,
    const StoragePartitionDescriptor& partition_descriptor,
    scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor) const {
  // We create per-app contexts on demand, unlike the others above.
  ChromeURLRequestContext* app_request_context =
      InitializeAppRequestContext(main_context, partition_descriptor,
                                  protocol_handler_interceptor.Pass());
  DCHECK(app_request_context);
  return app_request_context;
}

ChromeURLRequestContext*
OffTheRecordProfileIOData::AcquireIsolatedMediaRequestContext(
    ChromeURLRequestContext* app_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  NOTREACHED();
  return NULL;
}

chrome_browser_net::LoadTimeStats* OffTheRecordProfileIOData::GetLoadTimeStats(
    IOThread::Globals* io_thread_globals) const {
  return NULL;
}

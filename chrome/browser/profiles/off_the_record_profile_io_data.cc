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
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/server_bound_cert_service.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/http/http_server_properties_impl.h"
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
  if (main_request_context_getter_)
    main_request_context_getter_->CleanupOnUIThread();
  if (extensions_request_context_getter_)
    extensions_request_context_getter_->CleanupOnUIThread();

  // Clean up all isolated app request contexts.
  for (ChromeURLRequestContextGetterMap::iterator iter =
           app_request_context_getter_map_.begin();
       iter != app_request_context_getter_map_.end();
       ++iter) {
    iter->second->CleanupOnUIThread();
  }

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
    const std::string& app_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!app_id.empty());
  LazyInitialize();

  // Keep a map of request context getters, one per requested app ID.
  ChromeURLRequestContextGetterMap::iterator iter =
      app_request_context_getter_map_.find(app_id);
  if (iter != app_request_context_getter_map_.end())
    return iter->second;

  ChromeURLRequestContextGetter* context =
      ChromeURLRequestContextGetter::CreateOffTheRecordForIsolatedApp(
          profile_, io_data_, app_id);
  app_request_context_getter_map_[app_id] = context;

  return context;
}

void OffTheRecordProfileIOData::Handle::LazyInitialize() const {
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  ChromeNetworkDelegate::InitializeReferrersEnabled(
      io_data_->enable_referrers(), profile_->GetPrefs());
#if defined(ENABLE_SAFE_BROWSING)
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
      profile_->GetPrefs(), NULL);
  io_data_->safe_browsing_enabled()->MoveToThread(BrowserThread::IO);
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
  ChromeURLRequestContext* extensions_context = extensions_request_context();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  ApplyProfileParamsToContext(main_context);
  ApplyProfileParamsToContext(extensions_context);

  main_context->set_transport_security_state(transport_security_state());
  extensions_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());
  extensions_context->set_net_log(io_thread->net_log());

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
  extensions_context->set_throttler_manager(
      io_thread_globals->throttler_manager.get());

  // For incognito, we use the default non-persistent HttpServerPropertiesImpl.
  http_server_properties_.reset(new net::HttpServerPropertiesImpl);
  main_context->set_http_server_properties(http_server_properties_.get());

  // For incognito, we use a non-persistent server bound cert store.
  net::ServerBoundCertService* server_bound_cert_service =
      new net::ServerBoundCertService(
          new net::DefaultServerBoundCertStore(NULL),
          base::WorkerPool::GetTaskRunner(true));
  set_server_bound_cert_service(server_bound_cert_service);
  main_context->set_server_bound_cert_service(server_bound_cert_service);

  main_context->set_cookie_store(
      new net::CookieMonster(NULL, profile_params->cookie_monster_delegate));
  // All we care about for extensions is the cookie store. For incognito, we
  // use a non-persistent cookie store.

  net::CookieMonster* extensions_cookie_store =
      new net::CookieMonster(NULL, NULL);
  // Enable cookies for devtools and extension URLs.
  const char* schemes[] = {chrome::kChromeDevToolsScheme,
                           chrome::kExtensionScheme};
  extensions_cookie_store->SetCookieableSchemes(schemes, 2);
  extensions_context->set_cookie_store(extensions_cookie_store);

  net::HttpCache::BackendFactory* main_backend =
      net::HttpCache::DefaultBackend::InMemory(0);

  std::string trusted_spdy_proxy;
  if (command_line.HasSwitch(switches::kTrustedSpdyProxy)) {
    trusted_spdy_proxy = command_line.GetSwitchValueASCII(
        switches::kTrustedSpdyProxy);
  }

  net::HttpCache* cache =
      new net::HttpCache(main_context->host_resolver(),
                         main_context->cert_verifier(),
                         main_context->server_bound_cert_service(),
                         main_context->transport_security_state(),
                         main_context->proxy_service(),
                         GetSSLSessionCacheShard(),
                         main_context->ssl_config_service(),
                         main_context->http_auth_handler_factory(),
                         main_context->network_delegate(),
                         main_context->http_server_properties(),
                         main_context->net_log(),
                         main_backend,
                         trusted_spdy_proxy);

  main_http_factory_.reset(cache);
  main_context->set_http_transaction_factory(cache);
  ftp_factory_.reset(
      new net::FtpNetworkLayer(main_context->host_resolver()));
  main_context->set_ftp_transaction_factory(ftp_factory_.get());

  main_context->set_chrome_url_data_manager_backend(
      chrome_url_data_manager_backend());

  main_context->set_job_factory(job_factory());
  extensions_context->set_job_factory(job_factory());
}

ChromeURLRequestContext*
OffTheRecordProfileIOData::InitializeAppRequestContext(
    ChromeURLRequestContext* main_context,
    const std::string& app_id) const {
  AppRequestContext* context = new AppRequestContext;

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
  net::HttpCache* app_http_cache =
      new net::HttpCache(main_network_session, app_backend);

  context->SetHttpTransactionFactory(app_http_cache);
  return context;
}

ChromeURLRequestContext*
OffTheRecordProfileIOData::AcquireMediaRequestContext() const {
  NOTREACHED();
  return NULL;
}

ChromeURLRequestContext*
OffTheRecordProfileIOData::AcquireIsolatedAppRequestContext(
    ChromeURLRequestContext* main_context,
    const std::string& app_id) const {
  // We create per-app contexts on demand, unlike the others above.
  ChromeURLRequestContext* app_request_context =
      InitializeAppRequestContext(main_context, app_id);
  DCHECK(app_request_context);
  return app_request_context;
}

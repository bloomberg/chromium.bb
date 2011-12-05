// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/http_server_properties_manager.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/sqlite_origin_bound_cert_store.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/resource_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/origin_bound_cert_service.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_job_factory.h"

using content::BrowserThread;

namespace {

void ClearNetworkingHistorySinceOnIOThread(
    ProfileImplIOData* io_data, base::Time time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  io_data->transport_security_state()->DeleteSince(time);
  io_data->http_server_properties()->Clear();
}

}  // namespace

ProfileImplIOData::Handle::Handle(Profile* profile)
    : io_data_(new ProfileImplIOData),
      profile_(profile),
      initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
}

ProfileImplIOData::Handle::~Handle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (main_request_context_getter_)
    main_request_context_getter_->CleanupOnUIThread();
  if (media_request_context_getter_)
    media_request_context_getter_->CleanupOnUIThread();
  if (extensions_request_context_getter_)
    extensions_request_context_getter_->CleanupOnUIThread();

  if (io_data_->predictor_.get() != NULL) {
    // io_data_->predictor_ might be NULL if Init() was never called
    // (i.e. we shut down before ProfileImpl::DoFinalInit() got called).
    PrefService* user_prefs = profile_->GetPrefs();
    io_data_->predictor_->ShutdownOnUIThread(user_prefs);
  }

  // Clean up all isolated app request contexts.
  for (ChromeURLRequestContextGetterMap::iterator iter =
           app_request_context_getter_map_.begin();
       iter != app_request_context_getter_map_.end();
       ++iter) {
    iter->second->CleanupOnUIThread();
  }

  if (io_data_->http_server_properties_manager_.get())
    io_data_->http_server_properties_manager_->ShutdownOnUIThread();
  io_data_->ShutdownOnUIThread();
}

void ProfileImplIOData::Handle::Init(
      const FilePath& cookie_path,
      const FilePath& origin_bound_cert_path,
      const FilePath& cache_path,
      int cache_max_size,
      const FilePath& media_cache_path,
      int media_cache_max_size,
      const FilePath& extensions_cookie_path,
      const FilePath& app_path,
      chrome_browser_net::Predictor* predictor,
      PrefService* local_state,
      IOThread* io_thread,
      bool restore_old_session_cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!io_data_->lazy_params_.get());
  DCHECK(predictor);

  LazyParams* lazy_params = new LazyParams;

  lazy_params->cookie_path = cookie_path;
  lazy_params->origin_bound_cert_path = origin_bound_cert_path;
  lazy_params->cache_path = cache_path;
  lazy_params->cache_max_size = cache_max_size;
  lazy_params->media_cache_path = media_cache_path;
  lazy_params->media_cache_max_size = media_cache_max_size;
  lazy_params->extensions_cookie_path = extensions_cookie_path;
  lazy_params->restore_old_session_cookies = restore_old_session_cookies;

  io_data_->lazy_params_.reset(lazy_params);

  // Keep track of isolated app path separately so we can use it on demand.
  io_data_->app_path_ = app_path;

  io_data_->predictor_.reset(predictor);
  io_data_->predictor_->InitNetworkPredictor(profile_->GetPrefs(),
                                             local_state,
                                             io_thread);
}

base::Callback<ChromeURLDataManagerBackend*(void)>
ProfileImplIOData::Handle::GetChromeURLDataManagerBackendGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  return base::Bind(&ProfileIOData::GetChromeURLDataManagerBackend,
                    base::Unretained(io_data_));
}

const content::ResourceContext&
ProfileImplIOData::Handle::GetResourceContext() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Don't call LazyInitialize here, since the resource context is created at
  // the beginning of initalization and is used by some members while they're
  // being initialized (i.e. AppCacheService).
  return io_data_->GetResourceContext();
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetMainRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  if (!main_request_context_getter_) {
    main_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOriginal(
            profile_, io_data_);
  }
  return main_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetMediaRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  if (!media_request_context_getter_) {
    media_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOriginalForMedia(
            profile_, io_data_);
  }
  return media_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetExtensionsRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  if (!extensions_request_context_getter_) {
    extensions_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOriginalForExtensions(
            profile_, io_data_);
  }
  return extensions_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetIsolatedAppRequestContextGetter(
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
      ChromeURLRequestContextGetter::CreateOriginalForIsolatedApp(
          profile_, io_data_, app_id);
  app_request_context_getter_map_[app_id] = context;

  return context;
}

void ProfileImplIOData::Handle::ClearNetworkingHistorySince(
    base::Time time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ClearNetworkingHistorySinceOnIOThread,
          io_data_,
          time));
}

void ProfileImplIOData::Handle::LazyInitialize() const {
  if (!initialized_) {
    io_data_->InitializeOnUIThread(profile_);
    PrefService* pref_service = profile_->GetPrefs();
    io_data_->http_server_properties_manager_.reset(
        new chrome_browser_net::HttpServerPropertiesManager(pref_service));
    ChromeNetworkDelegate::InitializeReferrersEnabled(
        io_data_->enable_referrers(), pref_service);
    io_data_->clear_local_state_on_exit()->Init(
        prefs::kClearSiteDataOnExit, pref_service, NULL);
    io_data_->clear_local_state_on_exit()->MoveToThread(BrowserThread::IO);
#if defined(ENABLE_SAFE_BROWSING)
    io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
        pref_service, NULL);
    io_data_->safe_browsing_enabled()->MoveToThread(BrowserThread::IO);
#endif
    initialized_ = true;
  }
}

ProfileImplIOData::LazyParams::LazyParams()
    : cache_max_size(0),
      media_cache_max_size(0) {}
ProfileImplIOData::LazyParams::~LazyParams() {}

ProfileImplIOData::ProfileImplIOData()
    : ProfileIOData(false),
      clear_local_state_on_exit_(false) {}
ProfileImplIOData::~ProfileImplIOData() {}

void ProfileImplIOData::LazyInitializeInternal(
    ProfileParams* profile_params) const {
  // Keep track of clear_local_state_on_exit for isolated apps.
  clear_local_state_on_exit_ = profile_params->clear_local_state_on_exit;

  ChromeURLRequestContext* main_context = main_request_context();
  ChromeURLRequestContext* extensions_context = extensions_request_context();
  media_request_context_ = new ChromeURLRequestContext;

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool record_mode = chrome::kRecordModeEnabled &&
                     command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  // Initialize context members.

  ApplyProfileParamsToContext(main_context);
  ApplyProfileParamsToContext(media_request_context_);
  ApplyProfileParamsToContext(extensions_context);

  if (http_server_properties_manager_.get())
    http_server_properties_manager_->InitializeOnIOThread();

  main_context->set_transport_security_state(transport_security_state());
  media_request_context_->set_transport_security_state(
      transport_security_state());
  extensions_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());
  media_request_context_->set_net_log(io_thread->net_log());
  extensions_context->set_net_log(io_thread->net_log());

  main_context->set_network_delegate(network_delegate());
  media_request_context_->set_network_delegate(network_delegate());

  main_context->set_http_server_properties(http_server_properties());
  media_request_context_->set_http_server_properties(http_server_properties());

  main_context->set_host_resolver(
      io_thread_globals->host_resolver.get());
  media_request_context_->set_host_resolver(
      io_thread_globals->host_resolver.get());
  main_context->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  media_request_context_->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  main_context->set_dnsrr_resolver(
      io_thread_globals->dnsrr_resolver.get());
  media_request_context_->set_dnsrr_resolver(
      io_thread_globals->dnsrr_resolver.get());
  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());
  media_request_context_->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  main_context->set_dns_cert_checker(dns_cert_checker());
  main_context->set_fraudulent_certificate_reporter(
      fraudulent_certificate_reporter());
  media_request_context_->set_dns_cert_checker(dns_cert_checker());
  media_request_context_->set_fraudulent_certificate_reporter(
      fraudulent_certificate_reporter());

  main_context->set_proxy_service(proxy_service());
  media_request_context_->set_proxy_service(proxy_service());

  scoped_refptr<net::CookieStore> cookie_store = NULL;
  net::OriginBoundCertService* origin_bound_cert_service = NULL;
  if (record_mode || playback_mode) {
    // Don't use existing cookies and use an in-memory store.
    cookie_store = new net::CookieMonster(
        NULL, profile_params->cookie_monster_delegate);
    // Don't use existing origin-bound certs and use an in-memory store.
    origin_bound_cert_service = new net::OriginBoundCertService(
        new net::DefaultOriginBoundCertStore(NULL));
  }

  // setup cookie store
  if (!cookie_store) {
    DCHECK(!lazy_params_->cookie_path.empty());

    scoped_refptr<SQLitePersistentCookieStore> cookie_db =
        new SQLitePersistentCookieStore(
            lazy_params_->cookie_path,
            lazy_params_->restore_old_session_cookies);
    cookie_db->SetClearLocalStateOnExit(
        profile_params->clear_local_state_on_exit);
    cookie_store =
        new net::CookieMonster(cookie_db.get(),
                               profile_params->cookie_monster_delegate);
    if (command_line.HasSwitch(switches::kEnableRestoreSessionState))
      cookie_store->GetCookieMonster()->SetPersistSessionCookies(true);
  }

  net::CookieMonster* extensions_cookie_store =
      new net::CookieMonster(
          new SQLitePersistentCookieStore(
              lazy_params_->extensions_cookie_path,
              lazy_params_->restore_old_session_cookies), NULL);
  // Enable cookies for devtools and extension URLs.
  const char* schemes[] = {chrome::kChromeDevToolsScheme,
                           chrome::kExtensionScheme};
  extensions_cookie_store->SetCookieableSchemes(schemes, 2);

  main_context->set_cookie_store(cookie_store);
  media_request_context_->set_cookie_store(cookie_store);
  extensions_context->set_cookie_store(extensions_cookie_store);

  // Setup origin bound cert service.
  if (!origin_bound_cert_service) {
    DCHECK(!lazy_params_->origin_bound_cert_path.empty());

    scoped_refptr<SQLiteOriginBoundCertStore> origin_bound_cert_db =
        new SQLiteOriginBoundCertStore(lazy_params_->origin_bound_cert_path);
    origin_bound_cert_db->SetClearLocalStateOnExit(
        profile_params->clear_local_state_on_exit);
    origin_bound_cert_service = new net::OriginBoundCertService(
        new net::DefaultOriginBoundCertStore(origin_bound_cert_db.get()));
  }

  set_origin_bound_cert_service(origin_bound_cert_service);
  main_context->set_origin_bound_cert_service(origin_bound_cert_service);
  media_request_context_->set_origin_bound_cert_service(
      origin_bound_cert_service);

  net::HttpCache::DefaultBackend* main_backend =
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE,
          lazy_params_->cache_path,
          lazy_params_->cache_max_size,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  net::HttpCache* main_cache = new net::HttpCache(
      main_context->host_resolver(),
      main_context->cert_verifier(),
      main_context->origin_bound_cert_service(),
      main_context->dnsrr_resolver(),
      main_context->dns_cert_checker(),
      main_context->proxy_service(),
      main_context->ssl_config_service(),
      main_context->http_auth_handler_factory(),
      main_context->network_delegate(),
      main_context->http_server_properties(),
      main_context->net_log(),
      main_backend);

  net::HttpCache::DefaultBackend* media_backend =
      new net::HttpCache::DefaultBackend(
          net::MEDIA_CACHE, lazy_params_->media_cache_path,
          lazy_params_->media_cache_max_size,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  net::HttpNetworkSession* main_network_session = main_cache->GetSession();
  net::HttpCache* media_cache =
      new net::HttpCache(main_network_session, media_backend);

  if (record_mode || playback_mode) {
    main_cache->set_mode(
        record_mode ? net::HttpCache::RECORD : net::HttpCache::PLAYBACK);
  }

  main_http_factory_.reset(main_cache);
  media_http_factory_.reset(media_cache);
  main_context->set_http_transaction_factory(main_cache);
  media_request_context_->set_http_transaction_factory(media_cache);

  ftp_factory_.reset(
      new net::FtpNetworkLayer(io_thread_globals->host_resolver.get()));
  main_context->set_ftp_transaction_factory(ftp_factory_.get());

  main_context->set_chrome_url_data_manager_backend(
      chrome_url_data_manager_backend());

  main_context->set_job_factory(job_factory());
  media_request_context_->set_job_factory(job_factory());
  extensions_context->set_job_factory(job_factory());

  job_factory()->AddInterceptor(
      new chrome_browser_net::ConnectInterceptor(predictor_.get()));

  lazy_params_.reset();
}

net::HttpServerProperties* ProfileImplIOData::http_server_properties() const {
  return http_server_properties_manager_.get();
}

scoped_refptr<ChromeURLRequestContext>
ProfileImplIOData::InitializeAppRequestContext(
    scoped_refptr<ChromeURLRequestContext> main_context,
    const std::string& app_id) const {
  AppRequestContext* context = new AppRequestContext;

  // Copy most state from the main context.
  context->CopyFrom(main_context);

  FilePath app_path = app_path_.AppendASCII(app_id);
  FilePath cookie_path = app_path.Append(chrome::kCookieFilename);
  FilePath cache_path = app_path.Append(chrome::kCacheDirname);
  // TODO(creis): Determine correct cache size.
  int cache_max_size = 0;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool record_mode = chrome::kRecordModeEnabled &&
                     command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  // Use a separate HTTP disk cache for isolated apps.
  net::HttpCache::DefaultBackend* app_backend =
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE,
          cache_path,
          cache_max_size,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  net::HttpNetworkSession* main_network_session =
      main_http_factory_->GetSession();
  net::HttpCache* app_http_cache =
      new net::HttpCache(main_network_session, app_backend);

  scoped_refptr<net::CookieStore> cookie_store = NULL;
  if (record_mode || playback_mode) {
    // Don't use existing cookies and use an in-memory store.
    // TODO(creis): We should have a cookie delegate for notifying the cookie
    // extensions API, but we need to update it to understand isolated apps
    // first.
    cookie_store = new net::CookieMonster(NULL, NULL);
    app_http_cache->set_mode(
        record_mode ? net::HttpCache::RECORD : net::HttpCache::PLAYBACK);
  }

  // Use an app-specific cookie store.
  if (!cookie_store) {
    DCHECK(!cookie_path.empty());

    scoped_refptr<SQLitePersistentCookieStore> cookie_db =
        new SQLitePersistentCookieStore(cookie_path, false);
    cookie_db->SetClearLocalStateOnExit(clear_local_state_on_exit_);
    // TODO(creis): We should have a cookie delegate for notifying the cookie
    // extensions API, but we need to update it to understand isolated apps
    // first.
    cookie_store = new net::CookieMonster(cookie_db.get(), NULL);
  }

  context->SetCookieStore(cookie_store);
  context->SetHttpTransactionFactory(app_http_cache);

  return context;
}

scoped_refptr<ChromeURLRequestContext>
ProfileImplIOData::AcquireMediaRequestContext() const {
  DCHECK(media_request_context_);
  return media_request_context_;
}

scoped_refptr<ChromeURLRequestContext>
ProfileImplIOData::AcquireIsolatedAppRequestContext(
    scoped_refptr<ChromeURLRequestContext> main_context,
    const std::string& app_id) const {
  // We create per-app contexts on demand, unlike the others above.
  scoped_refptr<ChromeURLRequestContext> app_request_context =
      InitializeAppRequestContext(main_context, app_id);
  DCHECK(app_request_context);
  return app_request_context;
}

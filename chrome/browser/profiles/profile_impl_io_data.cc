// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/about_protocol_handler.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/clear_on_exit_policy.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/http_server_properties_manager.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/browser/net/sqlite_server_bound_cert_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_context.h"
#include "net/base/server_bound_cert_service.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "webkit/quota/special_storage_policy.h"

using content::BrowserThread;

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

  // Clean up all isolated media request contexts.
  for (ChromeURLRequestContextGetterMap::iterator iter =
           isolated_media_request_context_getter_map_.begin();
       iter != isolated_media_request_context_getter_map_.end();
       ++iter) {
    iter->second->CleanupOnUIThread();
  }

  if (io_data_->http_server_properties_manager())
    io_data_->http_server_properties_manager()->ShutdownOnUIThread();
  io_data_->ShutdownOnUIThread();
}

void ProfileImplIOData::Handle::Init(
      const FilePath& cookie_path,
      const FilePath& server_bound_cert_path,
      const FilePath& cache_path,
      int cache_max_size,
      const FilePath& media_cache_path,
      int media_cache_max_size,
      const FilePath& extensions_cookie_path,
      const FilePath& app_path,
      const FilePath& infinite_cache_path,
      chrome_browser_net::Predictor* predictor,
      PrefService* local_state,
      IOThread* io_thread,
      bool restore_old_session_cookies,
      quota::SpecialStoragePolicy* special_storage_policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!io_data_->lazy_params_.get());
  DCHECK(predictor);

  LazyParams* lazy_params = new LazyParams;

  lazy_params->cookie_path = cookie_path;
  lazy_params->server_bound_cert_path = server_bound_cert_path;
  lazy_params->cache_path = cache_path;
  lazy_params->cache_max_size = cache_max_size;
  lazy_params->media_cache_path = media_cache_path;
  lazy_params->media_cache_max_size = media_cache_max_size;
  lazy_params->extensions_cookie_path = extensions_cookie_path;
  lazy_params->infinite_cache_path = infinite_cache_path;
  lazy_params->restore_old_session_cookies = restore_old_session_cookies;
  lazy_params->special_storage_policy = special_storage_policy;

  io_data_->lazy_params_.reset(lazy_params);

  // Keep track of isolated app path and cache sizes separately so we can use
  // them on demand.
  io_data_->app_path_ = app_path;
  io_data_->app_cache_max_size_ = cache_max_size;
  io_data_->app_media_cache_max_size_ = media_cache_max_size;

  io_data_->predictor_.reset(predictor);

  if (!main_request_context_getter_) {
    main_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOriginal(
            profile_, io_data_);
  }
  io_data_->predictor_->InitNetworkPredictor(profile_->GetPrefs(),
                                             local_state,
                                             io_thread,
                                             main_request_context_getter_);

  io_data_->InitializeMetricsEnabledStateOnUIThread();
}

base::Callback<ChromeURLDataManagerBackend*(void)>
ProfileImplIOData::Handle::GetChromeURLDataManagerBackendGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  return base::Bind(&ProfileIOData::GetChromeURLDataManagerBackend,
                    base::Unretained(io_data_));
}

content::ResourceContext*
    ProfileImplIOData::Handle::GetResourceContext() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
  return GetResourceContextNoInit();
}

content::ResourceContext*
ProfileImplIOData::Handle::GetResourceContextNoInit() const {
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

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
        content::Source<Profile>(profile_),
        content::NotificationService::NoDetails());
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
  CHECK(!app_id.empty());
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

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetIsolatedMediaRequestContextGetter(
    const std::string& app_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We must have an app ID, or this will act like the default media context.
  CHECK(!app_id.empty());
  LazyInitialize();

  // Keep a map of request context getters, one per requested app ID.
  ChromeURLRequestContextGetterMap::iterator iter =
      isolated_media_request_context_getter_map_.find(app_id);
  if (iter != isolated_media_request_context_getter_map_.end())
    return iter->second;

  ChromeURLRequestContextGetter* context =
      ChromeURLRequestContextGetter::CreateOriginalForIsolatedMedia(
          profile_, io_data_, app_id);
  isolated_media_request_context_getter_map_[app_id] = context;

  return context;
}

void ProfileImplIOData::Handle::ClearNetworkingHistorySince(
    base::Time time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ProfileImplIOData::ClearNetworkingHistorySinceOnIOThread,
          base::Unretained(io_data_),
          time));
}

void ProfileImplIOData::Handle::LazyInitialize() const {
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  PrefService* pref_service = profile_->GetPrefs();
  io_data_->set_http_server_properties_manager(
      new chrome_browser_net::HttpServerPropertiesManager(pref_service));
  ChromeNetworkDelegate::InitializeReferrersEnabled(
      io_data_->enable_referrers(), pref_service);
  io_data_->session_startup_pref()->Init(
      prefs::kRestoreOnStartup, pref_service, NULL);
  io_data_->session_startup_pref()->MoveToThread(BrowserThread::IO);
#if defined(ENABLE_SAFE_BROWSING)
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
      pref_service, NULL);
  io_data_->safe_browsing_enabled()->MoveToThread(BrowserThread::IO);
#endif
  io_data_->InitializeOnUIThread(profile_);
}

ProfileImplIOData::LazyParams::LazyParams()
    : cache_max_size(0),
      media_cache_max_size(0),
      restore_old_session_cookies(false) {}

ProfileImplIOData::LazyParams::~LazyParams() {}

ProfileImplIOData::ProfileImplIOData()
    : ProfileIOData(false) {}
ProfileImplIOData::~ProfileImplIOData() {
  DestroyResourceContext();

  if (media_request_context_.get())
    media_request_context_->AssertNoURLRequests();
}

void ProfileImplIOData::LazyInitializeInternal(
    ProfileParams* profile_params) const {
  ChromeURLRequestContext* main_context = main_request_context();
  ChromeURLRequestContext* extensions_context = extensions_request_context();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // Only allow Record Mode if we are in a Debug build or where we are running
  // a cycle, and the user has limited control.
  bool record_mode = command_line.HasSwitch(switches::kRecordMode) &&
                     (chrome::kRecordModeEnabled ||
                      command_line.HasSwitch(switches::kVisitURLs));
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  // Initialize context members.

  ApplyProfileParamsToContext(main_context);
  ApplyProfileParamsToContext(extensions_context);

  if (http_server_properties_manager())
    http_server_properties_manager()->InitializeOnIOThread();

  main_context->set_transport_security_state(transport_security_state());
  extensions_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());
  extensions_context->set_net_log(io_thread->net_log());

  main_context->set_network_delegate(network_delegate());

  main_context->set_http_server_properties(http_server_properties_manager());

  main_context->set_host_resolver(
      io_thread_globals->host_resolver.get());
  main_context->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  main_context->set_fraudulent_certificate_reporter(
      fraudulent_certificate_reporter());

  main_context->set_throttler_manager(
      io_thread_globals->throttler_manager.get());
  extensions_context->set_throttler_manager(
      io_thread_globals->throttler_manager.get());

  main_context->set_proxy_service(proxy_service());

  scoped_refptr<net::CookieStore> cookie_store = NULL;
  net::ServerBoundCertService* server_bound_cert_service = NULL;
  if (record_mode || playback_mode) {
    // Don't use existing cookies and use an in-memory store.
    cookie_store = new net::CookieMonster(
        NULL, profile_params->cookie_monster_delegate);
    // Don't use existing server-bound certs and use an in-memory store.
    server_bound_cert_service = new net::ServerBoundCertService(
        new net::DefaultServerBoundCertStore(NULL),
        base::WorkerPool::GetTaskRunner(true));
  }

  // setup cookie store
  if (!cookie_store) {
    DCHECK(!lazy_params_->cookie_path.empty());

    scoped_refptr<SQLitePersistentCookieStore> cookie_db =
        new SQLitePersistentCookieStore(
            lazy_params_->cookie_path,
            lazy_params_->restore_old_session_cookies,
            new ClearOnExitPolicy(lazy_params_->special_storage_policy));
    cookie_store =
        new net::CookieMonster(cookie_db.get(),
                               profile_params->cookie_monster_delegate);
    cookie_store->GetCookieMonster()->SetPersistSessionCookies(true);
  }

  net::CookieMonster* extensions_cookie_store =
      new net::CookieMonster(
          new SQLitePersistentCookieStore(
              lazy_params_->extensions_cookie_path,
              lazy_params_->restore_old_session_cookies, NULL), NULL);
  // Enable cookies for devtools and extension URLs.
  const char* schemes[] = {chrome::kChromeDevToolsScheme,
                           chrome::kExtensionScheme};
  extensions_cookie_store->SetCookieableSchemes(schemes, 2);

  main_context->set_cookie_store(cookie_store);
  extensions_context->set_cookie_store(extensions_cookie_store);

  // Setup server bound cert service.
  if (!server_bound_cert_service) {
    DCHECK(!lazy_params_->server_bound_cert_path.empty());

    scoped_refptr<SQLiteServerBoundCertStore> server_bound_cert_db =
        new SQLiteServerBoundCertStore(
            lazy_params_->server_bound_cert_path,
            new ClearOnExitPolicy(lazy_params_->special_storage_policy));
    server_bound_cert_service = new net::ServerBoundCertService(
        new net::DefaultServerBoundCertStore(server_bound_cert_db.get()),
        base::WorkerPool::GetTaskRunner(true));
  }

  set_server_bound_cert_service(server_bound_cert_service);
  main_context->set_server_bound_cert_service(server_bound_cert_service);

  net::HttpCache::DefaultBackend* main_backend =
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE,
          lazy_params_->cache_path,
          lazy_params_->cache_max_size,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  net::HttpNetworkSession::Params network_session_params;
  PopulateNetworkSessionParams(profile_params, &network_session_params);
  net::HttpCache* main_cache = new net::HttpCache(
      network_session_params, main_backend);
  main_cache->InitializeInfiniteCache(lazy_params_->infinite_cache_path);

  if (record_mode || playback_mode) {
    main_cache->set_mode(
        record_mode ? net::HttpCache::RECORD : net::HttpCache::PLAYBACK);
  }

  main_http_factory_.reset(main_cache);
  main_context->set_http_transaction_factory(main_cache);

#if !defined(DISABLE_FTP_SUPPORT)
  ftp_factory_.reset(
      new net::FtpNetworkLayer(io_thread_globals->host_resolver.get()));
  main_context->set_ftp_transaction_factory(ftp_factory_.get());
#endif  // !defined(DISABLE_FTP_SUPPORT)

  main_context->set_chrome_url_data_manager_backend(
      chrome_url_data_manager_backend());

  // Create a media request context based on the main context, but using a
  // media cache.
  media_request_context_.reset(InitializeMediaRequestContext(main_context, ""));

  main_job_factory_.reset(new net::URLRequestJobFactoryImpl);
  media_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
  extensions_job_factory_.reset(new net::URLRequestJobFactoryImpl);

  int set_protocol = main_job_factory_->SetProtocolHandler(
      chrome::kFileScheme, new net::FileProtocolHandler());
  DCHECK(set_protocol);
  set_protocol = media_request_job_factory_->SetProtocolHandler(
      chrome::kFileScheme, new net::FileProtocolHandler());
  DCHECK(set_protocol);
  // TODO(shalev): The extensions_job_factory has a NULL NetworkDelegate.
  // Without a network_delegate, this protocol handler will never
  // handle file: requests, but as a side effect it makes
  // job_factory::IsHandledProtocol return true, which prevents attempts to
  // handle the protocol externally.
  set_protocol = extensions_job_factory_->SetProtocolHandler(
      chrome::kFileScheme, new net::FileProtocolHandler());
  DCHECK(set_protocol);

  set_protocol = main_job_factory_->SetProtocolHandler(
      chrome::kChromeDevToolsScheme,
      CreateDevToolsProtocolHandler(chrome_url_data_manager_backend(),
                                    network_delegate()));
  DCHECK(set_protocol);
  set_protocol = media_request_job_factory_->SetProtocolHandler(
      chrome::kChromeDevToolsScheme,
      CreateDevToolsProtocolHandler(chrome_url_data_manager_backend(),
                                    network_delegate()));
  DCHECK(set_protocol);
  set_protocol = extensions_job_factory_->SetProtocolHandler(
      chrome::kChromeDevToolsScheme,
      CreateDevToolsProtocolHandler(chrome_url_data_manager_backend(), NULL));
  DCHECK(set_protocol);

  net::URLRequestJobFactory* job_factories[3];
  job_factories[0] = main_job_factory_.get();
  job_factories[1] = media_request_job_factory_.get();
  job_factories[2] = extensions_job_factory_.get();

  net::FtpAuthCache* ftp_auth_caches[3];
  ftp_auth_caches[0] = main_context->ftp_auth_cache();
  ftp_auth_caches[1] = media_request_context_->ftp_auth_cache();
  ftp_auth_caches[2] = extensions_context->ftp_auth_cache();

  for (int i = 0; i < 3; i++) {
    SetUpJobFactoryDefaults(job_factories[i]);
    job_factories[i]->SetProtocolHandler(chrome::kAboutScheme,
                                         new net::AboutProtocolHandler());
    CreateFtpProtocolHandler(job_factories[i], ftp_auth_caches[i]);
    job_factories[i]->AddInterceptor(
        new chrome_browser_net::ConnectInterceptor(predictor_.get()));
  }

  main_context->set_job_factory(main_job_factory_.get());
  media_request_context_->set_job_factory(media_request_job_factory_.get());
  extensions_context->set_job_factory(extensions_job_factory_.get());

  lazy_params_.reset();
}

ChromeURLRequestContext*
ProfileImplIOData::InitializeAppRequestContext(
    ChromeURLRequestContext* main_context,
    const std::string& app_id) const {
  // If this is for a guest process, we should not persist cookies and http
  // cache.
  bool is_guest_process = (app_id.find("guest-") != std::string::npos);

  // Copy most state from the main context.
  AppRequestContext* context = new AppRequestContext(load_time_stats());
  context->CopyFrom(main_context);

  FilePath app_path = app_path_.AppendASCII(app_id);
  FilePath cookie_path = app_path.Append(chrome::kCookieFilename);
  FilePath cache_path = app_path.Append(chrome::kCacheDirname);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // Only allow Record Mode if we are in a Debug build or where we are running
  // a cycle, and the user has limited control.
  bool record_mode = command_line.HasSwitch(switches::kRecordMode) &&
                     (chrome::kRecordModeEnabled ||
                      command_line.HasSwitch(switches::kVisitURLs));
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  // Use a separate HTTP disk cache for isolated apps.
  net::HttpCache::BackendFactory* app_backend = NULL;
  if (is_guest_process) {
    app_backend = net::HttpCache::DefaultBackend::InMemory(0);
  } else {
    app_backend = new net::HttpCache::DefaultBackend(
        net::DISK_CACHE,
        cache_path,
        app_cache_max_size_,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  }
  net::HttpNetworkSession* main_network_session =
      main_http_factory_->GetSession();
  net::HttpCache* app_http_cache =
      new net::HttpCache(main_network_session, app_backend);

  scoped_refptr<net::CookieStore> cookie_store = NULL;
  if (is_guest_process) {
    cookie_store = new net::CookieMonster(NULL, NULL);
  } else if (record_mode || playback_mode) {
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
        new SQLitePersistentCookieStore(cookie_path, false, NULL);
    // TODO(creis): We should have a cookie delegate for notifying the cookie
    // extensions API, but we need to update it to understand isolated apps
    // first.
    cookie_store = new net::CookieMonster(cookie_db.get(), NULL);
  }

  // Transfer ownership of the cookies and cache to AppRequestContext.
  context->SetCookieStore(cookie_store);
  context->SetHttpTransactionFactory(app_http_cache);

  return context;
}

ChromeURLRequestContext*
ProfileImplIOData::InitializeMediaRequestContext(
    ChromeURLRequestContext* original_context,
    const std::string& app_id) const {
  // If this is for a guest process, we do not persist storage, so we can
  // simply use the app's in-memory cache (like off-the-record mode).
  if (app_id.find("guest-") != std::string::npos)
    return original_context;

  // Copy most state from the original context.
  MediaRequestContext* context = new MediaRequestContext(load_time_stats());
  context->CopyFrom(original_context);

  FilePath app_path = app_path_.AppendASCII(app_id);
  FilePath cache_path;
  int cache_max_size = app_media_cache_max_size_;
  if (app_id.empty()) {
    // lazy_params_ is only valid for the default media context creation.
    cache_path = lazy_params_->media_cache_path;
    cache_max_size = lazy_params_->media_cache_max_size;
  } else {
    cache_path = app_path.Append(chrome::kMediaCacheDirname);
  }

  // Use a separate HTTP disk cache for isolated apps.
  net::HttpCache::BackendFactory* media_backend =
      new net::HttpCache::DefaultBackend(
          net::MEDIA_CACHE,
          cache_path,
          cache_max_size,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  net::HttpNetworkSession* main_network_session =
      main_http_factory_->GetSession();
  net::HttpCache* media_http_cache =
      new net::HttpCache(main_network_session, media_backend);

  // Transfer ownership of the cache to MediaRequestContext.
  context->SetHttpTransactionFactory(media_http_cache);

  return context;
}

ChromeURLRequestContext*
ProfileImplIOData::AcquireMediaRequestContext() const {
  DCHECK(media_request_context_.get());
  return media_request_context_.get();
}

ChromeURLRequestContext*
ProfileImplIOData::AcquireIsolatedAppRequestContext(
    ChromeURLRequestContext* main_context,
    const std::string& app_id) const {
  // We create per-app contexts on demand, unlike the others above.
  ChromeURLRequestContext* app_request_context =
      InitializeAppRequestContext(main_context, app_id);
  DCHECK(app_request_context);
  return app_request_context;
}

ChromeURLRequestContext*
ProfileImplIOData::AcquireIsolatedMediaRequestContext(
    ChromeURLRequestContext* app_context,
    const std::string& app_id) const {
  // We create per-app media contexts on demand, unlike the others above.
  ChromeURLRequestContext* media_request_context =
      InitializeMediaRequestContext(app_context, app_id);
  DCHECK(media_request_context);
  return media_request_context;
}

chrome_browser_net::LoadTimeStats* ProfileImplIOData::GetLoadTimeStats(
    IOThread::Globals* io_thread_globals) const {
  return io_thread_globals->load_time_stats.get();
}

void ProfileImplIOData::CreateFtpProtocolHandler(
    net::URLRequestJobFactory* job_factory,
    net::FtpAuthCache* ftp_auth_cache) const {
#if !defined(DISABLE_FTP_SUPPORT)
  job_factory->SetProtocolHandler(
      chrome::kFtpScheme,
      new net::FtpProtocolHandler(ftp_factory_.get(),
                                  ftp_auth_cache));
#endif  // !defined(DISABLE_FTP_SUPPORT)
}

void ProfileImplIOData::ClearNetworkingHistorySinceOnIOThread(
    base::Time time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  LazyInitialize();

  DCHECK(transport_security_state());
  transport_security_state()->DeleteSince(time);
  DCHECK(http_server_properties_manager());
  http_server_properties_manager()->Clear();
}

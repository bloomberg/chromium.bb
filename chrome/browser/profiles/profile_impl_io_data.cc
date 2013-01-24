// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/prefs/public/pref_member.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/io_thread.h"
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
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/constants.h"
#include "net/base/server_bound_cert_service.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
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
  if (io_data_->predictor_.get() != NULL) {
    // io_data_->predictor_ might be NULL if Init() was never called
    // (i.e. we shut down before ProfileImpl::DoFinalInit() got called).
    PrefService* user_prefs = profile_->GetPrefs();
    io_data_->predictor_->ShutdownOnUIThread(user_prefs);
  }

  if (io_data_->http_server_properties_manager_)
    io_data_->http_server_properties_manager_->ShutdownOnUIThread();
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
      const FilePath& profile_path,
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

  // Keep track of profile path and cache sizes separately so we can use them
  // on demand when creating storage isolated URLRequestContextGetters.
  io_data_->profile_path_ = profile_path;
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
    const FilePath& partition_path,
    bool in_memory) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

  scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
      protocol_handler_interceptor(
          ProtocolHandlerRegistryFactory::GetForProfile(profile_)->
              CreateJobInterceptorFactory());
  ChromeURLRequestContextGetter* context =
      ChromeURLRequestContextGetter::CreateOriginalForIsolatedApp(
          profile_, io_data_, descriptor,
          protocol_handler_interceptor.Pass());
  app_request_context_getter_map_[descriptor] = context;

  return context;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileImplIOData::Handle::GetIsolatedMediaRequestContextGetter(
    const FilePath& partition_path,
    bool in_memory) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  ChromeURLRequestContextGetter* app_context =
      GetIsolatedAppRequestContextGetter(partition_path, in_memory);
  ChromeURLRequestContextGetter* context =
      ChromeURLRequestContextGetter::CreateOriginalForIsolatedMedia(
          profile_, app_context, io_data_, descriptor);
  isolated_media_request_context_getter_map_[descriptor] = context;

  return context;
}

void ProfileImplIOData::Handle::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ProfileImplIOData::ClearNetworkingHistorySinceOnIOThread,
          base::Unretained(io_data_),
          time,
          completion));
}

void ProfileImplIOData::Handle::LazyInitialize() const {
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  PrefService* pref_service = profile_->GetPrefs();
  io_data_->http_server_properties_manager_ =
      new chrome_browser_net::HttpServerPropertiesManager(pref_service);
  io_data_->set_http_server_properties(
      io_data_->http_server_properties_manager_);
  io_data_->session_startup_pref()->Init(
      prefs::kRestoreOnStartup, pref_service);
  io_data_->session_startup_pref()->MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
      pref_service);
  io_data_->safe_browsing_enabled()->MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
#endif
  io_data_->InitializeOnUIThread(profile_);
}

ProfileImplIOData::LazyParams::LazyParams()
    : cache_max_size(0),
      media_cache_max_size(0),
      restore_old_session_cookies(false) {}

ProfileImplIOData::LazyParams::~LazyParams() {}

ProfileImplIOData::ProfileImplIOData()
    : ProfileIOData(false),
      http_server_properties_manager_(NULL) {}
ProfileImplIOData::~ProfileImplIOData() {
  DestroyResourceContext();

  if (media_request_context_.get())
    media_request_context_->AssertNoURLRequests();
}

void ProfileImplIOData::LazyInitializeInternal(
    ProfileParams* profile_params) const {
  ChromeURLRequestContext* main_context = main_request_context();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // Only allow Record Mode if we are in a Debug build or where we are running
  // a cycle, and the user has limited control.
  bool record_mode = command_line.HasSwitch(switches::kRecordMode) &&
                     (chrome::kRecordModeEnabled ||
                      command_line.HasSwitch(switches::kVisitURLs));
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  network_delegate()->set_predictor(predictor_.get());

  // Initialize context members.

  ApplyProfileParamsToContext(main_context);

  if (http_server_properties_manager_)
    http_server_properties_manager_->InitializeOnIOThread();

  main_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());

  main_context->set_network_delegate(network_delegate());

  main_context->set_http_server_properties(http_server_properties());

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

  main_context->set_cookie_store(cookie_store);

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

  // Create a media request context based on the main context, but using a
  // media cache.  It shares the same job factory as the main context.
  StoragePartitionDescriptor details(profile_path_, false);
  media_request_context_.reset(InitializeMediaRequestContext(main_context,
                                                             details));

  lazy_params_.reset();
}

void ProfileImplIOData::
    InitializeExtensionsRequestContext(ProfileParams* profile_params) const {
  ChromeURLRequestContext* extensions_context = extensions_request_context();
  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  ApplyProfileParamsToContext(extensions_context);

  extensions_context->set_transport_security_state(transport_security_state());

  extensions_context->set_net_log(io_thread->net_log());

  extensions_context->set_throttler_manager(
      io_thread_globals->throttler_manager.get());

  net::CookieMonster* extensions_cookie_store =
      new net::CookieMonster(
          new SQLitePersistentCookieStore(
              lazy_params_->extensions_cookie_path,
              lazy_params_->restore_old_session_cookies, NULL), NULL);
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
  // SetUpJobFactory() to get this effect.
  extensions_job_factory_ = SetUpJobFactoryDefaults(
      extensions_job_factory.Pass(),
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>(NULL),
      NULL,
      extensions_context->ftp_transaction_factory(),
      extensions_context->ftp_auth_cache());
  extensions_context->set_job_factory(extensions_job_factory_.get());
}

ChromeURLRequestContext*
ProfileImplIOData::InitializeAppRequestContext(
    ChromeURLRequestContext* main_context,
    const StoragePartitionDescriptor& partition_descriptor,
    scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor) const {
  // Copy most state from the main context.
  AppRequestContext* context = new AppRequestContext(load_time_stats());
  context->CopyFrom(main_context);

  FilePath cookie_path = partition_descriptor.path.Append(
      chrome::kCookieFilename);
  FilePath cache_path = partition_descriptor.path.Append(chrome::kCacheDirname);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // Only allow Record Mode if we are in a Debug build or where we are running
  // a cycle, and the user has limited control.
  bool record_mode = command_line.HasSwitch(switches::kRecordMode) &&
                     (chrome::kRecordModeEnabled ||
                      command_line.HasSwitch(switches::kVisitURLs));
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  // Use a separate HTTP disk cache for isolated apps.
  net::HttpCache::BackendFactory* app_backend = NULL;
  if (partition_descriptor.in_memory) {
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
  if (partition_descriptor.in_memory) {
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
  context->SetHttpTransactionFactory(
      scoped_ptr<net::HttpTransactionFactory>(app_http_cache));

  scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  scoped_ptr<net::URLRequestJobFactory> top_job_factory;
  // Overwrite the job factory that we inherit from the main context so
  // that we can later provide our own handlers for storage related protocols.
  // Install all the usual protocol handlers unless we are in a browser plugin
  // guest process, in which case only web-safe schemes are allowed.
  if (!partition_descriptor.in_memory) {
    top_job_factory = SetUpJobFactoryDefaults(
        job_factory.Pass(), protocol_handler_interceptor.Pass(),
        network_delegate(),
        context->ftp_transaction_factory(),
        context->ftp_auth_cache());
  } else {
    top_job_factory = job_factory.PassAs<net::URLRequestJobFactory>();
  }
  context->SetJobFactory(top_job_factory.Pass());

  return context;
}

ChromeURLRequestContext*
ProfileImplIOData::InitializeMediaRequestContext(
    ChromeURLRequestContext* original_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  // If this is for a in_memory partition, we can simply use the original
  // context (like off-the-record mode).
  if (partition_descriptor.in_memory)
    return original_context;

  // Copy most state from the original context.
  MediaRequestContext* context = new MediaRequestContext(load_time_stats());
  context->CopyFrom(original_context);

  using content::StoragePartition;
  FilePath cache_path;
  int cache_max_size = app_media_cache_max_size_;
  if (partition_descriptor.path == profile_path_) {
    // lazy_params_ is only valid for the default media context creation.
    cache_path = lazy_params_->media_cache_path;
    cache_max_size = lazy_params_->media_cache_max_size;
  } else {
    cache_path = partition_descriptor.path.Append(chrome::kMediaCacheDirname);
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
  scoped_ptr<net::HttpTransactionFactory> media_http_cache(
      new net::HttpCache(main_network_session, media_backend));

  // Transfer ownership of the cache to MediaRequestContext.
  context->SetHttpTransactionFactory(media_http_cache.Pass());

  // Note that we do not create a new URLRequestJobFactory because
  // the media context should behave exactly like its parent context
  // in all respects except for cache behavior on media subresources.
  // The CopyFrom() step above means that our media context will use
  // the same URLRequestJobFactory instance that our parent context does.

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
ProfileImplIOData::AcquireIsolatedMediaRequestContext(
    ChromeURLRequestContext* app_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  // We create per-app media contexts on demand, unlike the others above.
  ChromeURLRequestContext* media_request_context =
      InitializeMediaRequestContext(app_context, partition_descriptor);
  DCHECK(media_request_context);
  return media_request_context;
}

chrome_browser_net::LoadTimeStats* ProfileImplIOData::GetLoadTimeStats(
    IOThread::Globals* io_thread_globals) const {
  return io_thread_globals->load_time_stats.get();
}

void ProfileImplIOData::ClearNetworkingHistorySinceOnIOThread(
    base::Time time,
    const base::Closure& completion) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  LazyInitialize();

  DCHECK(transport_security_state());
  transport_security_state()->DeleteSince(time);  // Completes synchronously.
  DCHECK(http_server_properties_manager_);
  http_server_properties_manager_->Clear(completion);
}

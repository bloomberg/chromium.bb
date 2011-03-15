// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl_io_data.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_cookie_policy.h"
#include "chrome/browser/net/chrome_dns_cert_provenance_checker_factory.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"

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
}

void ProfileImplIOData::Handle::Init(const FilePath& cookie_path,
                                     const FilePath& cache_path,
                                     int cache_max_size,
                                     const FilePath& media_cache_path,
                                     int media_cache_max_size,
                                     const FilePath& extensions_cookie_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!io_data_->lazy_params_.get());
  LazyParams* lazy_params = new LazyParams;

  lazy_params->cookie_path = cookie_path;
  lazy_params->cache_path = cache_path;
  lazy_params->cache_max_size = cache_max_size;
  lazy_params->media_cache_path = media_cache_path;
  lazy_params->media_cache_max_size = media_cache_max_size;
  lazy_params->extensions_cookie_path = extensions_cookie_path;

  lazy_params->io_thread = g_browser_process->io_thread();

  io_data_->lazy_params_.reset(lazy_params);
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

void ProfileImplIOData::Handle::LazyInitialize() const {
  if (!initialized_) {
    InitializeProfileParams(profile_, &io_data_->lazy_params_->profile_params);
    initialized_ = true;
  }
}

ProfileImplIOData::LazyParams::LazyParams()
    : cache_max_size(0),
      media_cache_max_size(0),
      io_thread(NULL) {}
ProfileImplIOData::LazyParams::~LazyParams() {}

ProfileImplIOData::ProfileImplIOData() : ProfileIOData(false) {}
ProfileImplIOData::~ProfileImplIOData() {}

void ProfileImplIOData::LazyInitializeInternal() const {
  main_request_context_ = new RequestContext;
  media_request_context_ = new RequestContext;
  extensions_request_context_ = new RequestContext;

  IOThread* const io_thread = lazy_params_->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  const ProfileParams& profile_params = lazy_params_->profile_params;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool record_mode = chrome::kRecordModeEnabled &&
                     command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  // Initialize context members.

  ApplyProfileParamsToContext(profile_params, main_request_context_);
  ApplyProfileParamsToContext(profile_params, media_request_context_);
  ApplyProfileParamsToContext(profile_params, extensions_request_context_);
  profile_params.appcache_service->set_request_context(main_request_context_);
  scoped_refptr<ChromeCookiePolicy> cookie_policy =
      new ChromeCookiePolicy(profile_params.host_content_settings_map);

  main_request_context_->set_chrome_cookie_policy(cookie_policy);
  media_request_context_->set_chrome_cookie_policy(cookie_policy);
  extensions_request_context_->set_chrome_cookie_policy(cookie_policy);

  main_request_context_->set_net_log(lazy_params_->io_thread->net_log());
  media_request_context_->set_net_log(lazy_params_->io_thread->net_log());
  extensions_request_context_->set_net_log(lazy_params_->io_thread->net_log());

  network_delegate_.reset(new ChromeNetworkDelegate(
        io_thread_globals->extension_event_router_forwarder.get(),
        profile_params.profile_id,
        profile_params.protocol_handler_registry));
  main_request_context_->set_network_delegate(network_delegate_.get());
  media_request_context_->set_network_delegate(network_delegate_.get());

  main_request_context_->set_host_resolver(
      io_thread_globals->host_resolver.get());
  media_request_context_->set_host_resolver(
      io_thread_globals->host_resolver.get());
  main_request_context_->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  media_request_context_->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  main_request_context_->set_dnsrr_resolver(
      io_thread_globals->dnsrr_resolver.get());
  media_request_context_->set_dnsrr_resolver(
      io_thread_globals->dnsrr_resolver.get());
  main_request_context_->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());
  media_request_context_->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  dns_cert_checker_.reset(
      CreateDnsCertProvenanceChecker(io_thread_globals->dnsrr_resolver.get(),
                                     main_request_context_));
  main_request_context_->set_dns_cert_checker(dns_cert_checker_.get());
  media_request_context_->set_dns_cert_checker(dns_cert_checker_.get());

  net::ProxyService* proxy_service =
      CreateProxyService(
          io_thread->net_log(),
          io_thread_globals->proxy_script_fetcher_context.get(),
          lazy_params_->profile_params.proxy_config_service.release(),
          command_line);
  main_request_context_->set_proxy_service(proxy_service);
  media_request_context_->set_proxy_service(proxy_service);

  net::HttpCache::DefaultBackend* main_backend =
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE,
          lazy_params_->cache_path,
          lazy_params_->cache_max_size,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  net::HttpCache* main_cache = new net::HttpCache(
      main_request_context_->host_resolver(),
      main_request_context_->cert_verifier(),
      main_request_context_->dnsrr_resolver(),
      main_request_context_->dns_cert_checker(),
      main_request_context_->proxy_service(),
      main_request_context_->ssl_config_service(),
      main_request_context_->http_auth_handler_factory(),
      main_request_context_->network_delegate(),
      main_request_context_->net_log(),
      main_backend);

  net::HttpCache::DefaultBackend* media_backend =
      new net::HttpCache::DefaultBackend(
          net::MEDIA_CACHE, lazy_params_->media_cache_path,
          lazy_params_->media_cache_max_size,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  net::HttpNetworkSession* main_network_session = main_cache->GetSession();
  net::HttpCache* media_cache =
      new net::HttpCache(main_network_session, media_backend);

  scoped_refptr<net::CookieStore> cookie_store = NULL;
  if (record_mode || playback_mode) {
    // Don't use existing cookies and use an in-memory store.
    cookie_store = new net::CookieMonster(
        NULL, profile_params.cookie_monster_delegate);
    main_cache->set_mode(
        record_mode ? net::HttpCache::RECORD : net::HttpCache::PLAYBACK);
  }

  // setup cookie store
  if (!cookie_store) {
    DCHECK(!lazy_params_->cookie_path.empty());

    scoped_refptr<SQLitePersistentCookieStore> cookie_db =
        new SQLitePersistentCookieStore(lazy_params_->cookie_path);
    cookie_db->SetClearLocalStateOnExit(
        profile_params.clear_local_state_on_exit);
    cookie_store =
        new net::CookieMonster(cookie_db.get(),
                               profile_params.cookie_monster_delegate);
  }

  net::CookieMonster* extensions_cookie_store =
      new net::CookieMonster(
          new SQLitePersistentCookieStore(
              lazy_params_->extensions_cookie_path), NULL);
  // Enable cookies for devtools and extension URLs.
  const char* schemes[] = {chrome::kChromeDevToolsScheme,
                           chrome::kExtensionScheme};
  extensions_cookie_store->SetCookieableSchemes(schemes, 2);

  main_request_context_->set_cookie_store(cookie_store);
  media_request_context_->set_cookie_store(cookie_store);
  extensions_request_context_->set_cookie_store(
      extensions_cookie_store);

  main_http_factory_.reset(main_cache);
  media_http_factory_.reset(media_cache);
  main_request_context_->set_http_transaction_factory(main_cache);
  media_request_context_->set_http_transaction_factory(media_cache);

  main_request_context_->set_ftp_transaction_factory(
      new net::FtpNetworkLayer(io_thread_globals->host_resolver.get()));

  lazy_params_.reset();
}

scoped_refptr<ChromeURLRequestContext>
ProfileImplIOData::AcquireMainRequestContext() const {
  DCHECK(main_request_context_);
  scoped_refptr<ChromeURLRequestContext> context = main_request_context_;
  main_request_context_->set_profile_io_data(this);
  main_request_context_ = NULL;
  return context;
}

scoped_refptr<ChromeURLRequestContext>
ProfileImplIOData::AcquireMediaRequestContext() const {
  DCHECK(media_request_context_);
  scoped_refptr<ChromeURLRequestContext> context = media_request_context_;
  media_request_context_->set_profile_io_data(this);
  media_request_context_ = NULL;
  return context;
}

scoped_refptr<ChromeURLRequestContext>
ProfileImplIOData::AcquireExtensionsRequestContext() const {
  DCHECK(extensions_request_context_);
  scoped_refptr<ChromeURLRequestContext> context = extensions_request_context_;
  extensions_request_context_->set_profile_io_data(this);
  extensions_request_context_ = NULL;
  return context;
}

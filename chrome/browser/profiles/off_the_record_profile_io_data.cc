// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_io_data.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/resource_context.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "webkit/database/database_tracker.h"

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

const content::ResourceContext&
OffTheRecordProfileIOData::Handle::GetResourceContext() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LazyInitialize();
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
  if (!initialized_) {
    io_data_->InitializeProfileParams(profile_);
    ChromeNetworkDelegate::InitializeReferrersEnabled(
        io_data_->enable_referrers(), profile_->GetPrefs());
    initialized_ = true;
  }
}

OffTheRecordProfileIOData::OffTheRecordProfileIOData()
    : ProfileIOData(true) {}
OffTheRecordProfileIOData::~OffTheRecordProfileIOData() {
  STLDeleteValues(&app_http_factory_map_);
}

void OffTheRecordProfileIOData::LazyInitializeInternal(
    ProfileParams* profile_params) const {
  ChromeURLRequestContext* main_context = main_request_context();
  ChromeURLRequestContext* extensions_context = extensions_request_context();

  IOThread* const io_thread = profile_params->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(main_context);
  ApplyProfileParamsToContext(extensions_context);

  main_context->set_cookie_policy(cookie_policy());
  extensions_context->set_cookie_policy(cookie_policy());

  main_context->set_net_log(io_thread->net_log());
  extensions_context->set_net_log(io_thread->net_log());

  main_context->set_network_delegate(network_delegate());

  main_context->set_host_resolver(
      io_thread_globals->host_resolver.get());
  main_context->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  main_context->set_dnsrr_resolver(
      io_thread_globals->dnsrr_resolver.get());
  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());
  main_context->set_dns_cert_checker(dns_cert_checker());
  main_context->set_proxy_service(proxy_service());

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

  extensions_context->set_cookie_store(
      new net::CookieMonster(NULL, NULL));

  net::HttpCache::BackendFactory* main_backend =
      net::HttpCache::DefaultBackend::InMemory(0);
  net::HttpCache* cache =
      new net::HttpCache(main_context->host_resolver(),
                         main_context->cert_verifier(),
                         main_context->dnsrr_resolver(),
                         main_context->dns_cert_checker(),
                         main_context->proxy_service(),
                         main_context->ssl_config_service(),
                         main_context->http_auth_handler_factory(),
                         main_context->network_delegate(),
                         main_context->net_log(),
                         main_backend);

  main_http_factory_.reset(cache);
  main_context->set_http_transaction_factory(cache);
  main_context->set_ftp_transaction_factory(
      new net::FtpNetworkLayer(main_context->host_resolver()));
}

scoped_refptr<ProfileIOData::RequestContext>
OffTheRecordProfileIOData::InitializeAppRequestContext(
    scoped_refptr<ChromeURLRequestContext> main_context,
    const std::string& app_id) const {
  scoped_refptr<ProfileIOData::RequestContext> context = new RequestContext;

  // Copy most state from the main context.
  context->CopyFrom(main_context);

  // Use a separate in-memory cookie store for the app.
  // TODO(creis): We should have a cookie delegate for notifying the cookie
  // extensions API, but we need to update it to understand isolated apps first.
  context->set_cookie_store(
      new net::CookieMonster(NULL, NULL));

  // Use a separate in-memory cache for the app.
  net::HttpCache::BackendFactory* app_backend =
      net::HttpCache::DefaultBackend::InMemory(0);
  net::HttpNetworkSession* main_network_session =
      main_http_factory_->GetSession();
  net::HttpCache* app_http_cache =
      new net::HttpCache(main_network_session, app_backend);

  // Keep track of app_http_cache to delete it when we go away.
  DCHECK(!app_http_factory_map_[app_id]);
  app_http_factory_map_[app_id] = app_http_cache;
  context->set_http_transaction_factory(app_http_cache);

  return context;
}

scoped_refptr<ChromeURLRequestContext>
OffTheRecordProfileIOData::AcquireMediaRequestContext() const {
  NOTREACHED();
  return NULL;
}

scoped_refptr<ChromeURLRequestContext>
OffTheRecordProfileIOData::AcquireIsolatedAppRequestContext(
    scoped_refptr<ChromeURLRequestContext> main_context,
    const std::string& app_id) const {
  // We create per-app contexts on demand, unlike the others above.
  scoped_refptr<RequestContext> app_request_context =
      InitializeAppRequestContext(main_context, app_id);
  DCHECK(app_request_context);
  app_request_context->set_profile_io_data(this);
  return app_request_context;
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_io_data.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_cookie_policy.h"
#include "chrome/browser/net/chrome_dns_cert_provenance_checker_factory.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/url_constants.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"

OffTheRecordProfileIOData::Handle::Handle(Profile* profile)
    : io_data_(new OffTheRecordProfileIOData),
      profile_(profile),
      initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  DCHECK(!io_data_->lazy_params_.get());
  LazyParams* lazy_params = new LazyParams;
  lazy_params->io_thread = g_browser_process->io_thread();
  io_data_->lazy_params_.reset(lazy_params);
}

OffTheRecordProfileIOData::Handle::~Handle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (main_request_context_getter_)
    main_request_context_getter_->CleanupOnUIThread();
  if (extensions_request_context_getter_)
    extensions_request_context_getter_->CleanupOnUIThread();
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::GetMainRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

void OffTheRecordProfileIOData::Handle::LazyInitialize() const {
  if (!initialized_) {
    InitializeProfileParams(profile_, &io_data_->lazy_params_->profile_params);
    initialized_ = true;
  }
}

OffTheRecordProfileIOData::LazyParams::LazyParams() : io_thread(NULL) {}
OffTheRecordProfileIOData::LazyParams::~LazyParams() {}

OffTheRecordProfileIOData::OffTheRecordProfileIOData()
    : ProfileIOData(true),
      initialized_(false) {}
OffTheRecordProfileIOData::~OffTheRecordProfileIOData() {}

void OffTheRecordProfileIOData::LazyInitializeInternal() const {
  main_request_context_ = new RequestContext;
  extensions_request_context_ = new RequestContext;

  IOThread* const io_thread = lazy_params_->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  const ProfileParams& profile_params = lazy_params_->profile_params;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  ApplyProfileParamsToContext(profile_params, main_request_context_);
  ApplyProfileParamsToContext(profile_params, extensions_request_context_);
  profile_params.appcache_service->set_request_context(main_request_context_);

  scoped_refptr<ChromeCookiePolicy> cookie_policy =
      new ChromeCookiePolicy(profile_params.host_content_settings_map);
  main_request_context_->set_chrome_cookie_policy(cookie_policy);
  extensions_request_context_->set_chrome_cookie_policy(cookie_policy);

  main_request_context_->set_net_log(lazy_params_->io_thread->net_log());
  extensions_request_context_->set_net_log(lazy_params_->io_thread->net_log());

  main_request_context_->set_host_resolver(
      io_thread_globals->host_resolver.get());
  main_request_context_->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  main_request_context_->set_dnsrr_resolver(
      io_thread_globals->dnsrr_resolver.get());
  // TODO(willchan): Enable this when we can support ExtensionIOEventRouter for
  // OTR profiles.
#if 0
  main_request_context_->set_network_delegate(
      &io_thread_globals->network_delegate);
#endif
  main_request_context_->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  dns_cert_checker_.reset(
      CreateDnsCertProvenanceChecker(io_thread_globals->dnsrr_resolver.get(),
                                     main_request_context_));
  main_request_context_->set_dns_cert_checker(dns_cert_checker_.get());

  main_request_context_->set_proxy_service(
      CreateProxyService(
          io_thread->net_log(),
          io_thread_globals->proxy_script_fetcher_context.get(),
          lazy_params_->profile_params.proxy_config_service.release(),
          command_line));

  main_request_context_->set_cookie_store(
      new net::CookieMonster(NULL, profile_params.cookie_monster_delegate));
  // All we care about for extensions is the cookie store. For incognito, we
  // use a non-persistent cookie store.

  net::CookieMonster* extensions_cookie_store =
      new net::CookieMonster(NULL, NULL);
  // Enable cookies for devtools and extension URLs.
  const char* schemes[] = {chrome::kChromeDevToolsScheme,
                           chrome::kExtensionScheme};
  extensions_cookie_store->SetCookieableSchemes(schemes, 2);

  extensions_request_context_->set_cookie_store(
      new net::CookieMonster(NULL, NULL));

  net::HttpCache::BackendFactory* main_backend =
      net::HttpCache::DefaultBackend::InMemory(0);
  net::HttpCache* cache =
      new net::HttpCache(main_request_context_->host_resolver(),
                         main_request_context_->cert_verifier(),
                         main_request_context_->dnsrr_resolver(),
                         main_request_context_->dns_cert_checker(),
                         main_request_context_->proxy_service(),
                         main_request_context_->ssl_config_service(),
                         main_request_context_->http_auth_handler_factory(),
                         main_request_context_->network_delegate(),
                         main_request_context_->net_log(),
                         main_backend);

  main_http_factory_.reset(cache);
  main_request_context_->set_http_transaction_factory(cache);
  main_request_context_->set_ftp_transaction_factory(
      new net::FtpNetworkLayer(main_request_context_->host_resolver()));
}

scoped_refptr<ChromeURLRequestContext>
OffTheRecordProfileIOData::AcquireMainRequestContext() const {
  DCHECK(main_request_context_);
  scoped_refptr<ChromeURLRequestContext> context = main_request_context_;
  main_request_context_->set_profile_io_data(this);
  main_request_context_ = NULL;
  return context;
}

scoped_refptr<ChromeURLRequestContext>
OffTheRecordProfileIOData::AcquireMediaRequestContext() const {
  NOTREACHED();
  return NULL;
}

scoped_refptr<ChromeURLRequestContext>
OffTheRecordProfileIOData::AcquireExtensionsRequestContext() const {
  DCHECK(extensions_request_context_);
  scoped_refptr<ChromeURLRequestContext> context = extensions_request_context_;
  extensions_request_context_->set_profile_io_data(this);
  extensions_request_context_ = NULL;
  return context;
}

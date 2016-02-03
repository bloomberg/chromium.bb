// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/off_the_record_chrome_browser_state_io_data.h"

#import <UIKit/UIkit.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/chrome_constants.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/net/cookie_util.h"
#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"
#include "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/web_thread.h"
#include "net/base/sdch_manager.h"
#include "net/disk_cache/disk_cache.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/sdch/sdch_owner.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace {

// Callback doing nothing, called by DoomIncognitoCache() below.
void DoNothing(int rv) {}

// Called by the notification center on memory warnings.
void OnMemoryWarningReceived(CFNotificationCenterRef center,
                             void* observer,
                             CFStringRef name,
                             const void* object,
                             CFDictionaryRef userInfo) {
  OffTheRecordChromeBrowserStateIOData::Handle* handle =
      (OffTheRecordChromeBrowserStateIOData::Handle*)observer;
  handle->DoomIncognitoCache();
}

}  // namespace

void OffTheRecordChromeBrowserStateIOData::Handle::DoomIncognitoCache() {
  // The cache for the incognito profile is in RAM.
  scoped_refptr<net::URLRequestContextGetter> getter =
      main_request_context_getter_;
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE, base::BindBlock(^{
        DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
        net::HttpCache* cache = getter->GetURLRequestContext()
                                    ->http_transaction_factory()
                                    ->GetCache();
        if (!cache->GetCurrentBackend())
          return;
        cache->GetCurrentBackend()->DoomAllEntries(base::Bind(&DoNothing));
      }));
}

OffTheRecordChromeBrowserStateIOData::Handle::Handle(
    ios::ChromeBrowserState* browser_state)
    : io_data_(new OffTheRecordChromeBrowserStateIOData()),
      browser_state_(browser_state),
      initialized_(false) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  DCHECK(browser_state);
  io_data_->cookie_path_ =
      browser_state->GetStatePath().Append(kIOSChromeCookieFilename);
  io_data_->channel_id_path_ =
      browser_state->GetStatePath().Append(kIOSChromeChannelIDFilename);
}

OffTheRecordChromeBrowserStateIOData::Handle::~Handle() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);

  // Stop listening to notifications.
  CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), this,
                                     nullptr, nullptr);

  io_data_->ShutdownOnUIThread(GetAllContextGetters());
}

scoped_refptr<IOSChromeURLRequestContextGetter>
OffTheRecordChromeBrowserStateIOData::Handle::CreateMainRequestContextGetter(
    ProtocolHandlerMap* protocol_handlers) const {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  LazyInitialize();
  DCHECK(!main_request_context_getter_.get());
  main_request_context_getter_ =
      IOSChromeURLRequestContextGetter::Create(io_data_, protocol_handlers);
  return main_request_context_getter_;
}

ChromeBrowserStateIOData*
OffTheRecordChromeBrowserStateIOData::Handle::io_data() const {
  LazyInitialize();
  return io_data_;
}

void OffTheRecordChromeBrowserStateIOData::Handle::LazyInitialize() const {
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
                                          browser_state_->GetPrefs());
  io_data_->safe_browsing_enabled()->MoveToThread(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO));
  io_data_->InitializeOnUIThread(browser_state_);

  // Once initialized, listen to memory warnings.
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(), this, &OnMemoryWarningReceived,
      static_cast<CFStringRef>(
          UIApplicationDidReceiveMemoryWarningNotification),
      nullptr, CFNotificationSuspensionBehaviorCoalesce);
}

scoped_ptr<ChromeBrowserStateIOData::IOSChromeURLRequestContextGetterVector>
OffTheRecordChromeBrowserStateIOData::Handle::GetAllContextGetters() {
  scoped_ptr<IOSChromeURLRequestContextGetterVector> context_getters(
      new IOSChromeURLRequestContextGetterVector());
  if (main_request_context_getter_.get())
    context_getters->push_back(main_request_context_getter_);

  return context_getters;
}

OffTheRecordChromeBrowserStateIOData::OffTheRecordChromeBrowserStateIOData()
    : ChromeBrowserStateIOData(
          ios::ChromeBrowserStateType::INCOGNITO_BROWSER_STATE) {}

OffTheRecordChromeBrowserStateIOData::~OffTheRecordChromeBrowserStateIOData() {}

void OffTheRecordChromeBrowserStateIOData::InitializeInternal(
    scoped_ptr<IOSChromeNetworkDelegate> chrome_network_delegate,
    ProfileParams* profile_params,
    ProtocolHandlerMap* protocol_handlers) const {
  net::URLRequestContext* main_context = main_request_context();

  IOSChromeIOThread* const io_thread = profile_params->io_thread;
  IOSChromeIOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(main_context);

  main_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());

  main_context->set_network_delegate(chrome_network_delegate.get());

  network_delegate_ = std::move(chrome_network_delegate);

  main_context->set_host_resolver(io_thread_globals->host_resolver.get());
  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());
  main_context->set_proxy_service(proxy_service());

  main_context->set_cert_transparency_verifier(
      io_thread_globals->cert_transparency_verifier.get());
  main_context->set_backoff_manager(
      io_thread_globals->url_request_backoff_manager.get());

  // For incognito, we use the default non-persistent HttpServerPropertiesImpl.
  set_http_server_properties(scoped_ptr<net::HttpServerProperties>(
      new net::HttpServerPropertiesImpl()));
  main_context->set_http_server_properties(http_server_properties());

  // For incognito, we use a non-persistent channel ID store.
  scoped_refptr<net::DefaultChannelIDStore::PersistentStore> channel_id_store;

  // On iOS, certificates are persisted to the disk in incognito.
  DCHECK(!channel_id_path_.empty());
  channel_id_store = new net::SQLiteChannelIDStore(
      channel_id_path_,
      web::WebThread::GetTaskRunnerForThread(web::WebThread::DB));

  net::ChannelIDService* channel_id_service = new net::ChannelIDService(
      new net::DefaultChannelIDStore(channel_id_store.get()),
      base::WorkerPool::GetTaskRunner(true));
  set_channel_id_service(channel_id_service);
  main_context->set_channel_id_service(channel_id_service);

  main_context->set_cookie_store(
      cookie_util::CreateCookieStore(cookie_util::CookieStoreConfig(
          cookie_path_,
          cookie_util::CookieStoreConfig::RESTORED_SESSION_COOKIES,
          cookie_util::CookieStoreConfig::COOKIE_STORE_IOS, nullptr)));

  http_network_session_ = CreateHttpNetworkSession(*profile_params);
  main_http_factory_ = CreateMainHttpFactory(
      http_network_session_.get(), net::HttpCache::DefaultBackend::InMemory(0));

  main_context->set_http_transaction_factory(main_http_factory_.get());

  scoped_ptr<net::URLRequestJobFactoryImpl> main_job_factory(
      new net::URLRequestJobFactoryImpl());

  InstallProtocolHandlers(main_job_factory.get(), protocol_handlers);
  URLRequestInterceptorScopedVector empty_interceptors;
  main_job_factory_ = SetUpJobFactoryDefaults(std::move(main_job_factory),
                                              std::move(empty_interceptors),
                                              main_context->network_delegate());
  main_context->set_job_factory(main_job_factory_.get());

  // Setup SDCH for this profile.
  sdch_manager_.reset(new net::SdchManager);
  sdch_policy_.reset(new net::SdchOwner(sdch_manager_.get(), main_context));
  main_context->set_sdch_manager(sdch_manager_.get());
}

net::URLRequestContext*
OffTheRecordChromeBrowserStateIOData::InitializeAppRequestContext(
    net::URLRequestContext* main_context) const {
  NOTREACHED();
  return nullptr;
}

net::URLRequestContext*
OffTheRecordChromeBrowserStateIOData::AcquireIsolatedAppRequestContext(
    net::URLRequestContext* main_context) const {
  NOTREACHED();
  return nullptr;
}

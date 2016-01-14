// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state_io_data.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/about_handler/about_protocol_handler.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/net_log/chrome_net_log.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_driver/pref_names.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/net/ios_chrome_http_user_agent_settings.h"
#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"
#include "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#include "ios/chrome/browser/net/proxy_service_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/web_thread.h"
#include "net/base/keygen_handler.h"
#include "net/base/network_quality_estimator.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_persister.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/certificate_report_sender.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace {

// For safe shutdown, must be called before the ChromeBrowserStateIOData is
// destroyed.
void NotifyContextGettersOfShutdownOnIO(
    scoped_ptr<ChromeBrowserStateIOData::IOSChromeURLRequestContextGetterVector>
        getters) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  ChromeBrowserStateIOData::IOSChromeURLRequestContextGetterVector::iterator
      iter;
  for (auto& chrome_context_getter : *getters)
    chrome_context_getter->NotifyContextShuttingDown();
}

}  // namespace

void ChromeBrowserStateIOData::InitializeOnUIThread(
    ios::ChromeBrowserState* browser_state) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  PrefService* pref_service = browser_state->GetPrefs();
  scoped_ptr<ProfileParams> params(new ProfileParams);
  params->path = browser_state->GetOriginalChromeBrowserState()->GetStatePath();

  params->io_thread = GetApplicationContext()->GetIOSChromeIOThread();

  params->cookie_settings =
      ios::CookieSettingsFactory::GetForBrowserState(browser_state);
  params->host_content_settings_map =
      ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state);
  params->ssl_config_service = browser_state->GetSSLConfigService();

  params->proxy_config_service =
      ios::ProxyServiceFactory::CreateProxyConfigService(
          browser_state->GetProxyConfigTracker());

  params->browser_state = browser_state;
  profile_params_.reset(params.release());

  IOSChromeNetworkDelegate::InitializePrefsOnUIThread(&enable_do_not_track_,
                                                      pref_service);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO);

  chrome_http_user_agent_settings_.reset(
      new IOSChromeHttpUserAgentSettings(pref_service));

  // These members are used only for sign in, which is not enabled
  // in incognito mode.  So no need to initialize them.
  if (!IsOffTheRecord()) {
    google_services_user_account_id_.Init(prefs::kGoogleServicesUserAccountId,
                                          pref_service);
    google_services_user_account_id_.MoveToThread(io_task_runner);

    sync_disabled_.Init(sync_driver::prefs::kSyncManaged, pref_service);
    sync_disabled_.MoveToThread(io_task_runner);

    signin_allowed_.Init(prefs::kSigninAllowed, pref_service);
    signin_allowed_.MoveToThread(io_task_runner);
  }

  initialized_on_UI_thread_ = true;
}

ChromeBrowserStateIOData::AppRequestContext::AppRequestContext() {}

void ChromeBrowserStateIOData::AppRequestContext::SetCookieStore(
    net::CookieStore* cookie_store) {
  cookie_store_ = cookie_store;
  set_cookie_store(cookie_store);
}

void ChromeBrowserStateIOData::AppRequestContext::SetHttpTransactionFactory(
    scoped_ptr<net::HttpTransactionFactory> http_factory) {
  http_factory_ = std::move(http_factory);
  set_http_transaction_factory(http_factory_.get());
}

void ChromeBrowserStateIOData::AppRequestContext::SetJobFactory(
    scoped_ptr<net::URLRequestJobFactory> job_factory) {
  job_factory_ = std::move(job_factory);
  set_job_factory(job_factory_.get());
}

ChromeBrowserStateIOData::AppRequestContext::~AppRequestContext() {
  AssertNoURLRequests();
}

ChromeBrowserStateIOData::ProfileParams::ProfileParams()
    : io_thread(nullptr), browser_state(nullptr) {}

ChromeBrowserStateIOData::ProfileParams::~ProfileParams() {}

ChromeBrowserStateIOData::ChromeBrowserStateIOData(
    ios::ChromeBrowserStateType browser_state_type)
    : initialized_(false),
      initialized_on_UI_thread_(false),
      browser_state_type_(browser_state_type) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
}

ChromeBrowserStateIOData::~ChromeBrowserStateIOData() {
  if (web::WebThread::IsMessageLoopValid(web::WebThread::IO))
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  // Pull the contents of the request context maps onto the stack for sanity
  // checking of values in a minidump. http://crbug.com/260425
  size_t num_app_contexts = app_request_context_map_.size();
  size_t current_context = 0;
  static const size_t kMaxCachedContexts = 20;
  net::URLRequestContext* app_context_cache[kMaxCachedContexts] = {0};
  void* app_context_vtable_cache[kMaxCachedContexts] = {0};
  void* tmp_vtable = nullptr;
  base::debug::Alias(&num_app_contexts);
  base::debug::Alias(&current_context);
  base::debug::Alias(app_context_cache);
  base::debug::Alias(app_context_vtable_cache);
  base::debug::Alias(&tmp_vtable);

  current_context = 0;
  for (URLRequestContextMap::const_iterator
           it = app_request_context_map_.begin();
       current_context < kMaxCachedContexts &&
       it != app_request_context_map_.end();
       ++it, ++current_context) {
    app_context_cache[current_context] = it->second;
    memcpy(&app_context_vtable_cache[current_context],
           static_cast<void*>(it->second), sizeof(void*));
  }

  // Destroy certificate_report_sender_ before main_request_context_,
  // since the former has a reference to the latter.
  if (transport_security_state_)
    transport_security_state_->SetReportSender(nullptr);
  certificate_report_sender_.reset();

  // TODO(ajwong): These AssertNoURLRequests() calls are unnecessary since they
  // are already done in the URLRequestContext destructor.
  if (main_request_context_)
    main_request_context_->AssertNoURLRequests();

  current_context = 0;
  for (URLRequestContextMap::iterator it = app_request_context_map_.begin();
       it != app_request_context_map_.end(); ++it) {
    if (current_context < kMaxCachedContexts) {
      CHECK_EQ(app_context_cache[current_context], it->second);
      memcpy(&tmp_vtable, static_cast<void*>(it->second), sizeof(void*));
      CHECK_EQ(app_context_vtable_cache[current_context], tmp_vtable);
    }
    it->second->AssertNoURLRequests();
    delete it->second;
    current_context++;
  }
}

// static
bool ChromeBrowserStateIOData::IsHandledProtocol(const std::string& scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
      url::kFileScheme, kChromeUIScheme, url::kDataScheme, url::kAboutScheme,
  };
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }
  return net::URLRequest::IsHandledProtocol(scheme);
}

// static
void ChromeBrowserStateIOData::InstallProtocolHandlers(
    net::URLRequestJobFactoryImpl* job_factory,
    ProtocolHandlerMap* protocol_handlers) {
  for (ProtocolHandlerMap::iterator it = protocol_handlers->begin();
       it != protocol_handlers->end(); ++it) {
    bool set_protocol = job_factory->SetProtocolHandler(
        it->first, make_scoped_ptr(it->second.release()));
    DCHECK(set_protocol);
  }
  protocol_handlers->clear();
}

net::URLRequestContext* ChromeBrowserStateIOData::GetMainRequestContext()
    const {
  DCHECK(initialized_);
  return main_request_context_.get();
}

net::URLRequestContext* ChromeBrowserStateIOData::GetIsolatedAppRequestContext(
    net::URLRequestContext* main_context,
    const base::FilePath& partition_path) const {
  DCHECK(initialized_);
  net::URLRequestContext* context = nullptr;
  if (ContainsKey(app_request_context_map_, partition_path)) {
    context = app_request_context_map_[partition_path];
  } else {
    context = AcquireIsolatedAppRequestContext(main_context);
    app_request_context_map_[partition_path] = context;
  }
  DCHECK(context);
  return context;
}

content_settings::CookieSettings* ChromeBrowserStateIOData::GetCookieSettings()
    const {
  DCHECK(initialized_);
  return cookie_settings_.get();
}

HostContentSettingsMap* ChromeBrowserStateIOData::GetHostContentSettingsMap()
    const {
  DCHECK(initialized_);
  return host_content_settings_map_.get();
}

bool ChromeBrowserStateIOData::IsOffTheRecord() const {
  return browser_state_type() ==
         ios::ChromeBrowserStateType::INCOGNITO_BROWSER_STATE;
}

void ChromeBrowserStateIOData::InitializeMetricsEnabledStateOnUIThread() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  // Prep the PrefMember and send it to the IO thread, since this value will be
  // read from there.
  enable_metrics_.Init(metrics::prefs::kMetricsReportingEnabled,
                       GetApplicationContext()->GetLocalState());
  enable_metrics_.MoveToThread(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO));
}

bool ChromeBrowserStateIOData::GetMetricsEnabledStateOnIOThread() const {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  return enable_metrics_.GetValue();
}

bool ChromeBrowserStateIOData::IsDataReductionProxyEnabled() const {
  return data_reduction_proxy_io_data() &&
         data_reduction_proxy_io_data()->IsEnabled();
}

void ChromeBrowserStateIOData::set_data_reduction_proxy_io_data(
    scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
        data_reduction_proxy_io_data) const {
  data_reduction_proxy_io_data_ = std::move(data_reduction_proxy_io_data);
}

base::WeakPtr<net::HttpServerProperties>
ChromeBrowserStateIOData::http_server_properties() const {
  return http_server_properties_->GetWeakPtr();
}

void ChromeBrowserStateIOData::set_http_server_properties(
    scoped_ptr<net::HttpServerProperties> http_server_properties) const {
  http_server_properties_ = std::move(http_server_properties);
}

void ChromeBrowserStateIOData::Init(
    ProtocolHandlerMap* protocol_handlers) const {
  // The basic logic is implemented here. The specific initialization
  // is done in InitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  DCHECK(!initialized_);

  // TODO(jhawkins): Remove once crbug.com/102004 is fixed.
  CHECK(initialized_on_UI_thread_);

  // TODO(jhawkins): Return to DCHECK once crbug.com/102004 is fixed.
  CHECK(profile_params_.get());

  IOSChromeIOThread* const io_thread = profile_params_->io_thread;
  IOSChromeIOThread::Globals* const io_thread_globals = io_thread->globals();

  // Create the common request contexts.
  main_request_context_.reset(new net::URLRequestContext());

  scoped_ptr<IOSChromeNetworkDelegate> network_delegate(
      new IOSChromeNetworkDelegate());

  network_delegate->set_cookie_settings(profile_params_->cookie_settings.get());
  network_delegate->set_enable_do_not_track(&enable_do_not_track_);

  // NOTE: Proxy service uses the default io thread network delegate, not the
  // delegate just created.
  proxy_service_ = ios::ProxyServiceFactory::CreateProxyService(
      io_thread->net_log(), nullptr,
      io_thread_globals->system_network_delegate.get(),
      std::move(profile_params_->proxy_config_service),
      true /* quick_check_enabled */);
  transport_security_state_.reset(new net::TransportSecurityState());
  base::SequencedWorkerPool* pool = web::WebThread::GetBlockingPool();
  transport_security_persister_.reset(new net::TransportSecurityPersister(
      transport_security_state_.get(), profile_params_->path,
      pool->GetSequencedTaskRunnerWithShutdownBehavior(
          pool->GetSequenceToken(), base::SequencedWorkerPool::BLOCK_SHUTDOWN),
      IsOffTheRecord()));

  certificate_report_sender_.reset(new net::CertificateReportSender(
      main_request_context_.get(),
      net::CertificateReportSender::DO_NOT_SEND_COOKIES));
  transport_security_state_->SetReportSender(certificate_report_sender_.get());

  // Take ownership over these parameters.
  cookie_settings_ = profile_params_->cookie_settings;
  host_content_settings_map_ = profile_params_->host_content_settings_map;

  main_request_context_->set_cert_verifier(
      io_thread_globals->cert_verifier.get());

  InitializeInternal(std::move(network_delegate), profile_params_.get(),
                     protocol_handlers);

  profile_params_.reset();
  initialized_ = true;
}

void ChromeBrowserStateIOData::ApplyProfileParamsToContext(
    net::URLRequestContext* context) const {
  context->set_http_user_agent_settings(chrome_http_user_agent_settings_.get());
  context->set_ssl_config_service(profile_params_->ssl_config_service.get());
}

scoped_ptr<net::URLRequestJobFactory>
ChromeBrowserStateIOData::SetUpJobFactoryDefaults(
    scoped_ptr<net::URLRequestJobFactoryImpl> job_factory,
    URLRequestInterceptorScopedVector request_interceptors,
    net::NetworkDelegate* network_delegate) const {
  // NOTE(willchan): Keep these protocol handlers in sync with
  // ChromeBrowserStateIOData::IsHandledProtocol().
  bool set_protocol = job_factory->SetProtocolHandler(
      url::kFileScheme,
      make_scoped_ptr(new net::FileProtocolHandler(
          web::WebThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN))));
  DCHECK(set_protocol);

  set_protocol = job_factory->SetProtocolHandler(
      url::kDataScheme, make_scoped_ptr(new net::DataProtocolHandler()));
  DCHECK(set_protocol);

  job_factory->SetProtocolHandler(
      url::kAboutScheme,
      make_scoped_ptr(new about_handler::AboutProtocolHandler()));

  // Set up interceptors in the reverse order.
  scoped_ptr<net::URLRequestJobFactory> top_job_factory =
      std::move(job_factory);
  for (URLRequestInterceptorScopedVector::reverse_iterator i =
           request_interceptors.rbegin();
       i != request_interceptors.rend(); ++i) {
    top_job_factory.reset(new net::URLRequestInterceptingJobFactory(
        std::move(top_job_factory), make_scoped_ptr(*i)));
  }
  request_interceptors.weak_clear();
  return top_job_factory;
}

void ChromeBrowserStateIOData::ShutdownOnUIThread(
    scoped_ptr<IOSChromeURLRequestContextGetterVector> context_getters) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);

  google_services_user_account_id_.Destroy();
  enable_referrers_.Destroy();
  enable_do_not_track_.Destroy();
  enable_metrics_.Destroy();
  safe_browsing_enabled_.Destroy();
  sync_disabled_.Destroy();
  signin_allowed_.Destroy();
  if (chrome_http_user_agent_settings_)
    chrome_http_user_agent_settings_->CleanupOnUIThread();

  if (!context_getters->empty()) {
    if (web::WebThread::IsMessageLoopValid(web::WebThread::IO)) {
      web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                               base::Bind(&NotifyContextGettersOfShutdownOnIO,
                                          base::Passed(&context_getters)));
    }
  }

  bool posted = web::WebThread::DeleteSoon(web::WebThread::IO, FROM_HERE, this);
  if (!posted)
    delete this;
}

void ChromeBrowserStateIOData::set_channel_id_service(
    net::ChannelIDService* channel_id_service) const {
  channel_id_service_.reset(channel_id_service);
}

scoped_ptr<net::HttpNetworkSession>
ChromeBrowserStateIOData::CreateHttpNetworkSession(
    const ProfileParams& profile_params) const {
  net::HttpNetworkSession::Params params;
  net::URLRequestContext* context = main_request_context();

  IOSChromeIOThread* const io_thread = profile_params.io_thread;

  io_thread->InitializeNetworkSessionParams(&params);
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(context,
                                                                 &params);
  if (!IsOffTheRecord()) {
    params.socket_performance_watcher_factory =
        io_thread->globals()->network_quality_estimator.get();
  }
  if (data_reduction_proxy_io_data_.get())
    params.proxy_delegate = data_reduction_proxy_io_data_->proxy_delegate();

  return scoped_ptr<net::HttpNetworkSession>(
      new net::HttpNetworkSession(params));
}

scoped_ptr<net::HttpCache> ChromeBrowserStateIOData::CreateMainHttpFactory(
    net::HttpNetworkSession* session,
    scoped_ptr<net::HttpCache::BackendFactory> main_backend) const {
  return scoped_ptr<net::HttpCache>(
      new net::HttpCache(session, std::move(main_backend), true));
}

scoped_ptr<net::HttpCache> ChromeBrowserStateIOData::CreateHttpFactory(
    net::HttpNetworkSession* shared_session,
    scoped_ptr<net::HttpCache::BackendFactory> backend) const {
  return scoped_ptr<net::HttpCache>(
      new net::HttpCache(shared_session, std::move(backend), true));
}

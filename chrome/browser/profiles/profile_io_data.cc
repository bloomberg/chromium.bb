// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/chrome_fraudulent_certificate_reporter.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/http_server_properties_manager.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/policy/url_blacklist_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/transport_security_persister.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_context.h"
#include "net/base/server_bound_cert_service.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/gdata/gdata_protocol_handler.h"
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserContext;
using content::BrowserThread;
using content::ResourceContext;

namespace {

// ----------------------------------------------------------------------------
// CookieMonster::Delegate implementation
// ----------------------------------------------------------------------------
class ChromeCookieMonsterDelegate : public net::CookieMonster::Delegate {
 public:
  explicit ChromeCookieMonsterDelegate(
      const base::Callback<Profile*(void)>& profile_getter)
      : profile_getter_(profile_getter) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  // net::CookieMonster::Delegate implementation.
  virtual void OnCookieChanged(
      const net::CookieMonster::CanonicalCookie& cookie,
      bool removed,
      net::CookieMonster::Delegate::ChangeCause cause) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeCookieMonsterDelegate::OnCookieChangedAsyncHelper,
                   this, cookie, removed, cause));
  }

 private:
  virtual ~ChromeCookieMonsterDelegate() {}

  void OnCookieChangedAsyncHelper(
      const net::CookieMonster::CanonicalCookie& cookie,
      bool removed,
      net::CookieMonster::Delegate::ChangeCause cause) {
    Profile* profile = profile_getter_.Run();
    if (profile) {
      ChromeCookieDetails cookie_details(&cookie, removed, cause);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_COOKIE_CHANGED,
          content::Source<Profile>(profile),
          content::Details<ChromeCookieDetails>(&cookie_details));
    }
  }

  const base::Callback<Profile*(void)> profile_getter_;
};

class ProtocolHandlerRegistryInterceptor
    : public net::URLRequestJobFactory::Interceptor {
 public:
  explicit ProtocolHandlerRegistryInterceptor(
      ProtocolHandlerRegistry* protocol_handler_registry)
      : protocol_handler_registry_(protocol_handler_registry) {
    DCHECK(protocol_handler_registry_);
  }

  virtual ~ProtocolHandlerRegistryInterceptor() {}

  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request) const OVERRIDE {
    return protocol_handler_registry_->MaybeCreateJob(request);
  }

  virtual bool WillHandleProtocol(const std::string& protocol) const {
    return protocol_handler_registry_->IsHandledProtocolIO(protocol);
  }

  virtual net::URLRequestJob* MaybeInterceptRedirect(
      const GURL& url, net::URLRequest* request) const OVERRIDE {
    return NULL;
  }

  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request) const OVERRIDE {
    return NULL;
  }

 private:
  const scoped_refptr<ProtocolHandlerRegistry> protocol_handler_registry_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerRegistryInterceptor);
};

Profile* GetProfileOnUI(ProfileManager* profile_manager, Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  if (profile_manager->IsValidProfile(profile))
    return profile;
  return NULL;
}

}  // namespace

void ProfileIOData::InitializeOnUIThread(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* pref_service = profile->GetPrefs();

  scoped_ptr<ProfileParams> params(new ProfileParams);
  params->path = profile->GetPath();
  params->clear_local_state_on_exit =
      pref_service->GetBoolean(prefs::kClearSiteDataOnExit);

  // Set up Accept-Language and Accept-Charset header values
  params->accept_language = net::HttpUtil::GenerateAcceptLanguageHeader(
      pref_service->GetString(prefs::kAcceptLanguages));
  std::string default_charset =
      pref_service->GetString(prefs::kGlobalDefaultCharset);
  params->accept_charset =
      net::HttpUtil::GenerateAcceptCharsetHeader(default_charset);

  // At this point, we don't know the charset of the referring page
  // where a url request originates from. This is used to get a suggested
  // filename from Content-Disposition header made of raw 8bit characters.
  // Down the road, it can be overriden if it becomes known (for instance,
  // when download request is made through the context menu in a web page).
  // At the moment, it'll remain 'undeterministic' when a user
  // types a URL in the omnibar or click on a download link in a page.
  // For the latter, we need a change on the webkit-side.
  // We initialize it to the default charset here and a user will
  // have an *arguably* better default charset for interpreting a raw 8bit
  // C-D header field.  It means the native OS codepage fallback in
  // net_util::GetSuggestedFilename is unlikely to be taken.
  params->referrer_charset = default_charset;

  params->io_thread = g_browser_process->io_thread();

  params->cookie_settings = CookieSettings::Factory::GetForProfile(profile);
  params->ssl_config_service = profile->GetSSLConfigService();
  base::Callback<Profile*(void)> profile_getter =
      base::Bind(&GetProfileOnUI, g_browser_process->profile_manager(),
                 profile);
  params->cookie_monster_delegate =
      new ChromeCookieMonsterDelegate(profile_getter);
  params->extension_info_map =
      ExtensionSystem::Get(profile)->info_map();

#if defined(ENABLE_NOTIFICATIONS)
  params->notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
#endif

  params->protocol_handler_registry = profile->GetProtocolHandlerRegistry();

  ChromeProxyConfigService* proxy_config_service =
      ProxyServiceFactory::CreateProxyConfigService(true);
  params->proxy_config_service.reset(proxy_config_service);
  profile->GetProxyConfigTracker()->SetChromeProxyConfigService(
      proxy_config_service);
  params->profile = profile;
  profile_params_.reset(params.release());

  // The URLBlacklistManager has to be created on the UI thread to register
  // observers of |pref_service|, and it also has to clean up on
  // ShutdownOnUIThread to release these observers on the right thread.
  // Don't pass it in |profile_params_| to make sure it is correctly cleaned up,
  // in particular when this ProfileIOData isn't |initialized_| during deletion.
#if defined(ENABLE_CONFIGURATION_POLICY)
  url_blacklist_manager_.reset(new policy::URLBlacklistManager(pref_service));
#endif

  initialized_on_UI_thread_ = true;

  // We need to make sure that content initializes its own data structures that
  // are associated with each ResourceContext because we might post this
  // object to the IO thread after this function.
  BrowserContext::EnsureResourceContextInitialized(profile);
}

ProfileIOData::AppRequestContext::AppRequestContext() {}

void ProfileIOData::AppRequestContext::SetCookieStore(
    net::CookieStore* cookie_store) {
  cookie_store_ = cookie_store;
  set_cookie_store(cookie_store);
}

void ProfileIOData::AppRequestContext::SetHttpTransactionFactory(
    net::HttpTransactionFactory* http_factory) {
  http_factory_.reset(http_factory);
  set_http_transaction_factory(http_factory);
}

ProfileIOData::AppRequestContext::~AppRequestContext() {}

ProfileIOData::ProfileParams::ProfileParams()
    : clear_local_state_on_exit(false),
      io_thread(NULL),
#if defined(ENABLE_NOTIFICATIONS)
      notification_service(NULL),
#endif
      profile(NULL) {
}

ProfileIOData::ProfileParams::~ProfileParams() {}

ProfileIOData::ProfileIOData(bool is_incognito)
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          resource_context_(new ResourceContext(this))),
      initialized_on_UI_thread_(false),
      is_incognito_(is_incognito) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ProfileIOData::~ProfileIOData() {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO))
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (main_request_context_)
    main_request_context_->AssertNoURLRequests();
  if (extensions_request_context_)
    extensions_request_context_->AssertNoURLRequests();
  for (AppRequestContextMap::iterator it = app_request_context_map_.begin();
       it != app_request_context_map_.end(); ++it) {
    it->second->AssertNoURLRequests();
  }
}

// static
ProfileIOData* ProfileIOData::FromResourceContext(
    content::ResourceContext* rc) {
  return (static_cast<ResourceContext*>(rc))->io_data_;
}

// static
bool ProfileIOData::IsHandledProtocol(const std::string& scheme) {
  DCHECK_EQ(scheme, StringToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
    chrome::kExtensionScheme,
    chrome::kChromeUIScheme,
    chrome::kChromeDevToolsScheme,
#if defined(OS_CHROMEOS)
    chrome::kMetadataScheme,
    chrome::kDriveScheme,
#endif  // defined(OS_CHROMEOS)
    chrome::kBlobScheme,
    chrome::kFileSystemScheme,
  };
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }
  return net::URLRequest::IsHandledProtocol(scheme);
}

bool ProfileIOData::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.scheme());
}

content::ResourceContext* ProfileIOData::GetResourceContext() const {
  return resource_context_.get();
}

ChromeURLDataManagerBackend*
ProfileIOData::GetChromeURLDataManagerBackend() const {
  LazyInitialize();
  return chrome_url_data_manager_backend_.get();
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetMainRequestContext() const {
  LazyInitialize();
  return main_request_context_;
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetMediaRequestContext() const {
  LazyInitialize();
  scoped_refptr<ChromeURLRequestContext> context =
      AcquireMediaRequestContext();
  DCHECK(context);
  return context;
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetExtensionsRequestContext() const {
  LazyInitialize();
  return extensions_request_context_;
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetIsolatedAppRequestContext(
    scoped_refptr<ChromeURLRequestContext> main_context,
    const std::string& app_id) const {
  LazyInitialize();
  scoped_refptr<ChromeURLRequestContext> context;
  if (ContainsKey(app_request_context_map_, app_id)) {
    context = app_request_context_map_[app_id];
  } else {
    context = AcquireIsolatedAppRequestContext(main_context, app_id);
    app_request_context_map_[app_id] = context;
  }
  DCHECK(context);
  return context;
}

ExtensionInfoMap* ProfileIOData::GetExtensionInfoMap() const {
  return extension_info_map_;
}

CookieSettings* ProfileIOData::GetCookieSettings() const {
  return cookie_settings_;
}

#if defined(ENABLE_NOTIFICATIONS)
DesktopNotificationService* ProfileIOData::GetNotificationService() const {
  return notification_service_;
}
#endif

void ProfileIOData::InitializeMetricsEnabledStateOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_CHROMEOS)
  // Just fetch the value from ChromeOS' settings while we're on the UI thread.
  // TODO(stevet): For now, this value is only set on profile initialization.
  // We will want to do something similar to the PrefMember method below in the
  // future to more accurately capture this state.
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &enable_metrics_);
#else
  // Prep the PrefMember and send it to the IO thread, since this value will be
  // read from there.
  enable_metrics_.Init(prefs::kMetricsReportingEnabled,
                       g_browser_process->local_state(),
                       NULL);
  enable_metrics_.MoveToThread(BrowserThread::IO);
#endif  // defined(OS_CHROMEOS)
}

bool ProfileIOData::GetMetricsEnabledStateOnIOThread() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
#if defined(OS_CHROMEOS)
  return enable_metrics_;
#else
  return enable_metrics_.GetValue();
#endif  // defined(OS_CHROMEOS)
}

chrome_browser_net::HttpServerPropertiesManager*
    ProfileIOData::http_server_properties_manager() const {
  return http_server_properties_manager_.get();
}

void ProfileIOData::set_http_server_properties_manager(
    chrome_browser_net::HttpServerPropertiesManager* manager) const {
  http_server_properties_manager_.reset(manager);
}

ProfileIOData::ResourceContext::ResourceContext(ProfileIOData* io_data)
    : io_data_(io_data) {
  DCHECK(io_data);
}

ProfileIOData::ResourceContext::~ResourceContext() {}

void ProfileIOData::ResourceContext::EnsureInitialized() {
  io_data_->LazyInitialize();
}

net::HostResolver* ProfileIOData::ResourceContext::GetHostResolver()  {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return host_resolver_;
}

net::URLRequestContext* ProfileIOData::ResourceContext::GetRequestContext()  {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return request_context_;
}

// static
std::string ProfileIOData::GetSSLSessionCacheShard() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // The SSL session cache is partitioned by setting a string. This returns a
  // unique string to partition the SSL session cache. Each time we create a
  // new profile, we'll get a fresh SSL session cache which is separate from
  // the other profiles.
  static unsigned ssl_session_cache_instance = 0;
  return StringPrintf("profile/%u", ssl_session_cache_instance++);
}

void ProfileIOData::LazyInitialize() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (initialized_)
    return;

  // TODO(jhawkins): Remove once crbug.com/102004 is fixed.
  CHECK(initialized_on_UI_thread_);

  // TODO(jhawkins): Return to DCHECK once crbug.com/102004 is fixed.
  CHECK(profile_params_.get());

  IOThread* const io_thread = profile_params_->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Create the common request contexts.
  main_request_context_ = new ChromeURLRequestContext;
  extensions_request_context_ = new ChromeURLRequestContext;

  chrome_url_data_manager_backend_.reset(new ChromeURLDataManagerBackend);

  network_delegate_.reset(new ChromeNetworkDelegate(
        io_thread_globals->extension_event_router_forwarder.get(),
        profile_params_->extension_info_map,
        url_blacklist_manager_.get(),
        profile_params_->profile,
        profile_params_->cookie_settings,
        &enable_referrers_));

  fraudulent_certificate_reporter_.reset(
      new chrome_browser_net::ChromeFraudulentCertificateReporter(
          main_request_context_));

  proxy_service_.reset(
      ProxyServiceFactory::CreateProxyService(
          io_thread->net_log(),
          io_thread_globals->proxy_script_fetcher_context.get(),
          profile_params_->proxy_config_service.release(),
          command_line));

  transport_security_state_.reset(new net::TransportSecurityState());
  transport_security_persister_.reset(
      new TransportSecurityPersister(transport_security_state_.get(),
                                     profile_params_->path,
                                     is_incognito()));
  const std::string& serialized =
      command_line.GetSwitchValueASCII(switches::kHstsHosts);
  transport_security_persister_.get()->DeserializeFromCommandLine(serialized);

  // NOTE(willchan): Keep these protocol handlers in sync with
  // ProfileIOData::IsHandledProtocol().
  job_factory_.reset(new net::URLRequestJobFactory);
  if (profile_params_->protocol_handler_registry) {
    job_factory_->AddInterceptor(
        new ProtocolHandlerRegistryInterceptor(
            profile_params_->protocol_handler_registry));
  }
  bool set_protocol = job_factory_->SetProtocolHandler(
      chrome::kExtensionScheme,
      CreateExtensionProtocolHandler(is_incognito(),
                                     profile_params_->extension_info_map));
  DCHECK(set_protocol);
  set_protocol = job_factory_->SetProtocolHandler(
      chrome::kChromeUIScheme,
      ChromeURLDataManagerBackend::CreateProtocolHandler(
          chrome_url_data_manager_backend_.get()));
  DCHECK(set_protocol);
  set_protocol = job_factory_->SetProtocolHandler(
      chrome::kChromeDevToolsScheme,
      CreateDevToolsProtocolHandler(chrome_url_data_manager_backend_.get()));
  DCHECK(set_protocol);
#if defined(OS_CHROMEOS)
  if (!is_incognito()) {
    set_protocol = job_factory_->SetProtocolHandler(
        chrome::kDriveScheme, new gdata::GDataProtocolHandler());
    DCHECK(set_protocol);
  }
#if !defined(GOOGLE_CHROME_BUILD)
  // Install the GView request interceptor that will redirect requests
  // of compatible documents (PDF, etc) to the GView document viewer.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(switches::kEnableGView))
    job_factory_->AddInterceptor(new chromeos::GViewRequestInterceptor);
#endif  // !defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_CHROMEOS)

  // Take ownership over these parameters.
  cookie_settings_ = profile_params_->cookie_settings;
#if defined(ENABLE_NOTIFICATIONS)
  notification_service_ = profile_params_->notification_service;
#endif
  extension_info_map_ = profile_params_->extension_info_map;

  resource_context_->host_resolver_ = io_thread_globals->host_resolver.get();
  resource_context_->request_context_ = main_request_context_;

  LazyInitializeInternal(profile_params_.get());

  profile_params_.reset();
  initialized_ = true;
}

void ProfileIOData::ApplyProfileParamsToContext(
    ChromeURLRequestContext* context) const {
  context->set_is_incognito(is_incognito());
  context->set_accept_language(profile_params_->accept_language);
  context->set_accept_charset(profile_params_->accept_charset);
  context->set_referrer_charset(profile_params_->referrer_charset);
  context->set_ssl_config_service(profile_params_->ssl_config_service);
}

void ProfileIOData::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enable_referrers_.Destroy();
#if !defined(OS_CHROMEOS)
  enable_metrics_.Destroy();
#endif
  clear_local_state_on_exit_.Destroy();
  safe_browsing_enabled_.Destroy();
  session_startup_pref_.Destroy();
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (url_blacklist_manager_.get())
    url_blacklist_manager_->ShutdownOnUIThread();
#endif
  bool posted = BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
  if (!posted)
    delete this;
}

void ProfileIOData::set_server_bound_cert_service(
    net::ServerBoundCertService* server_bound_cert_service) const {
  server_bound_cert_service_.reset(server_bound_cert_service);
}

void ProfileIOData::DestroyResourceContext() {
  resource_context_.reset();
}

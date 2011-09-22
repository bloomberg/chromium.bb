// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/chrome_dns_cert_provenance_checker_factory.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/policy/url_blacklist_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/transport_security_persister.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/browser_thread.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/resource_context.h"
#include "content/common/notification_service.h"
#include "net/base/origin_bound_cert_service.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_url_request_job_factory.h"
#include "webkit/database/database_tracker.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url_request_job_factory.h"
#include "webkit/quota/quota_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#endif  // defined(OS_CHROMEOS)

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
        NewRunnableMethod(this,
            &ChromeCookieMonsterDelegate::OnCookieChangedAsyncHelper,
            cookie,
            removed,
            cause));
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
      NotificationService::current()->Notify(
          chrome::NOTIFICATION_COOKIE_CHANGED,
          Source<Profile>(profile),
          Details<ChromeCookieDetails>(&cookie_details));
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

class ChromeBlobProtocolHandler : public webkit_blob::BlobProtocolHandler {
 public:
  ChromeBlobProtocolHandler(
      webkit_blob::BlobStorageController* blob_storage_controller,
      base::MessageLoopProxy* loop_proxy)
      : webkit_blob::BlobProtocolHandler(blob_storage_controller,
                                         loop_proxy) {}

  virtual ~ChromeBlobProtocolHandler() {}

 private:
  virtual scoped_refptr<webkit_blob::BlobData>
      LookupBlobData(net::URLRequest* request) const {
    ResourceDispatcherHostRequestInfo* info =
        ResourceDispatcherHost::InfoForRequest(request);
    if (!info)
      return NULL;
    return info->requested_blob_data();
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeBlobProtocolHandler);
};

Profile* GetProfileOnUI(ProfileManager* profile_manager, Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  if (profile_manager->IsValidProfile(profile))
    return profile;
  return NULL;
}

prerender::PrerenderManager* GetPrerenderManagerOnUI(
    const base::Callback<Profile*(void)>& profile_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = profile_getter.Run();
  if (profile)
    return profile->GetPrerenderManager();
  return NULL;
}

}  // namespace

void ProfileIOData::InitializeOnUIThread(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* pref_service = profile->GetPrefs();

  next_download_id_thunk_ = profile->GetDownloadManager()->GetNextIdThunk();

  scoped_ptr<ProfileParams> params(new ProfileParams);
  params->path = profile->GetPath();
  params->is_incognito = profile->IsOffTheRecord();
  params->clear_local_state_on_exit =
      pref_service->GetBoolean(prefs::kClearSiteDataOnExit);

  params->appcache_service = profile->GetAppCacheService();

  // Set up Accept-Language and Accept-Charset header values
  params->accept_language = net::HttpUtil::GenerateAcceptLanguageHeader(
      pref_service->GetString(prefs::kAcceptLanguages));
  std::string default_charset = pref_service->GetString(prefs::kDefaultCharset);
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

  params->host_content_settings_map = profile->GetHostContentSettingsMap();
  params->host_zoom_map = profile->GetHostZoomMap();
  params->ssl_config_service = profile->GetSSLConfigService();
  base::Callback<Profile*(void)> profile_getter =
      base::Bind(&GetProfileOnUI, g_browser_process->profile_manager(),
                 profile);
  params->cookie_monster_delegate =
      new ChromeCookieMonsterDelegate(profile_getter);
  params->database_tracker = profile->GetDatabaseTracker();
  params->appcache_service = profile->GetAppCacheService();
  params->blob_storage_context = profile->GetBlobStorageContext();
  params->file_system_context = profile->GetFileSystemContext();
  params->quota_manager = profile->GetQuotaManager();
  params->extension_info_map = profile->GetExtensionInfoMap();
  params->notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  params->prerender_manager_getter =
      base::Bind(&GetPrerenderManagerOnUI, profile_getter);
  params->protocol_handler_registry = profile->GetProtocolHandlerRegistry();

  params->proxy_config_service.reset(
      ProxyServiceFactory::CreateProxyConfigService(
          profile->GetProxyConfigTracker()));
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
}

ProfileIOData::AppRequestContext::AppRequestContext() {}
ProfileIOData::AppRequestContext::~AppRequestContext() {}

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

ProfileIOData::ProfileParams::ProfileParams()
    : is_incognito(false),
      clear_local_state_on_exit(false),
      io_thread(NULL),
      notification_service(NULL),
      profile(NULL) {}
ProfileIOData::ProfileParams::~ProfileParams() {}

ProfileIOData::ProfileIOData(bool is_incognito)
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(resource_context_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ProfileIOData::~ProfileIOData() {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO))
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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

const content::ResourceContext& ProfileIOData::GetResourceContext() const {
  return resource_context_;
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

HostContentSettingsMap* ProfileIOData::GetHostContentSettingsMap() const {
  return host_content_settings_map_;
}

DesktopNotificationService* ProfileIOData::GetNotificationService() const {
  return notification_service_;
}

ProfileIOData::ResourceContext::ResourceContext(const ProfileIOData* io_data)
    : io_data_(io_data) {
  DCHECK(io_data);
}

ProfileIOData::ResourceContext::~ResourceContext() {}

void ProfileIOData::ResourceContext::EnsureInitialized() const {
  io_data_->LazyInitialize();
}

void ProfileIOData::LazyInitialize() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (initialized_)
    return;
  DCHECK(profile_params_.get());

  IOThread* const io_thread = profile_params_->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Create the common request contexts.
  main_request_context_ = new ChromeURLRequestContext;
  extensions_request_context_ = new ChromeURLRequestContext;

  profile_params_->appcache_service->set_request_context(main_request_context_);

  chrome_url_data_manager_backend_.reset(new ChromeURLDataManagerBackend);

  network_delegate_.reset(new ChromeNetworkDelegate(
        io_thread_globals->extension_event_router_forwarder.get(),
        profile_params_->extension_info_map,
        url_blacklist_manager_.get(),
        profile_params_->profile,
        &enable_referrers_));

  dns_cert_checker_.reset(
      CreateDnsCertProvenanceChecker(io_thread_globals->dnsrr_resolver.get(),
                                     main_request_context_));

  proxy_service_.reset(
      ProxyServiceFactory::CreateProxyService(
          io_thread->net_log(),
          io_thread_globals->proxy_script_fetcher_context.get(),
          profile_params_->proxy_config_service.release(),
          command_line));

  transport_security_state_ = new net::TransportSecurityState(
      command_line.GetSwitchValueASCII(switches::kHstsHosts));
  transport_security_persister_.reset(
      new TransportSecurityPersister(transport_security_state_.get(),
                                     profile_params_->path,
                                     !profile_params_->is_incognito));

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
      CreateExtensionProtocolHandler(profile_params_->is_incognito,
                                     profile_params_->extension_info_map));
  DCHECK(set_protocol);
  set_protocol = job_factory_->SetProtocolHandler(
      chrome::kChromeUIScheme,
      ChromeURLDataManagerBackend::CreateProtocolHandler(
          chrome_url_data_manager_backend_.get(),
          profile_params_->appcache_service,
          profile_params_->blob_storage_context->controller()));
  DCHECK(set_protocol);
  set_protocol = job_factory_->SetProtocolHandler(
      chrome::kChromeDevToolsScheme,
      CreateDevToolsProtocolHandler(chrome_url_data_manager_backend_.get()));
  DCHECK(set_protocol);
  set_protocol = job_factory_->SetProtocolHandler(
      chrome::kBlobScheme,
      new ChromeBlobProtocolHandler(
          profile_params_->blob_storage_context->controller(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  DCHECK(set_protocol);
  set_protocol = job_factory_->SetProtocolHandler(
      chrome::kFileSystemScheme,
      CreateFileSystemProtocolHandler(
          profile_params_->file_system_context,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  DCHECK(set_protocol);
#if defined(OS_CHROMEOS)
  // Install the GView request interceptor that will redirect requests
  // of compatible documents (PDF, etc) to the GView document viewer.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(switches::kEnableGView))
    job_factory_->AddInterceptor(new chromeos::GViewRequestInterceptor);
#endif  // defined(OS_CHROMEOS)

  media_stream_manager_.reset(new media_stream::MediaStreamManager);

  // Take ownership over these parameters.
  database_tracker_ = profile_params_->database_tracker;
  appcache_service_ = profile_params_->appcache_service;
  blob_storage_context_ = profile_params_->blob_storage_context;
  file_system_context_ = profile_params_->file_system_context;
  quota_manager_ = profile_params_->quota_manager;
  host_zoom_map_ = profile_params_->host_zoom_map;
  host_content_settings_map_ = profile_params_->host_content_settings_map;
  notification_service_ = profile_params_->notification_service;
  extension_info_map_ = profile_params_->extension_info_map;
  prerender_manager_getter_ = profile_params_->prerender_manager_getter;

  resource_context_.set_host_resolver(io_thread_globals->host_resolver.get());
  resource_context_.set_request_context(main_request_context_);
  resource_context_.set_database_tracker(database_tracker_);
  resource_context_.set_appcache_service(appcache_service_);
  resource_context_.set_blob_storage_context(blob_storage_context_);
  resource_context_.set_file_system_context(file_system_context_);
  resource_context_.set_quota_manager(quota_manager_);
  resource_context_.set_host_zoom_map(host_zoom_map_);
  resource_context_.set_prerender_manager_getter(prerender_manager_getter_);
  resource_context_.SetUserData(NULL, const_cast<ProfileIOData*>(this));
  resource_context_.set_media_observer(
      io_thread_globals->media.media_internals.get());
  resource_context_.set_next_download_id_thunk(next_download_id_thunk_);
  resource_context_.set_media_stream_manager(media_stream_manager_.get());

  LazyInitializeInternal(profile_params_.get());

  profile_params_.reset();
  initialized_ = true;
}

void ProfileIOData::ApplyProfileParamsToContext(
    ChromeURLRequestContext* context) const {
  context->set_is_incognito(profile_params_->is_incognito);
  context->set_accept_language(profile_params_->accept_language);
  context->set_accept_charset(profile_params_->accept_charset);
  context->set_referrer_charset(profile_params_->referrer_charset);
  context->set_ssl_config_service(profile_params_->ssl_config_service);
}

void ProfileIOData::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enable_referrers_.Destroy();
  clear_local_state_on_exit_.Destroy();
  safe_browsing_enabled_.Destroy();
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (url_blacklist_manager_.get())
    url_blacklist_manager_->ShutdownOnUIThread();
#endif
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ResourceDispatcherHost::CancelRequestsForContext,
          base::Unretained(g_browser_process->resource_dispatcher_host()),
          &resource_context_));
  bool posted = BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                                        new DeleteTask<ProfileIOData>(this));
  if (!posted)
    delete this;
}

void ProfileIOData::set_origin_bound_cert_service(
    net::OriginBoundCertService* origin_bound_cert_service) const {
  origin_bound_cert_service_.reset(origin_bound_cert_service);
}

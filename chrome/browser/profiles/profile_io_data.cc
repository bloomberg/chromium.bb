// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/chrome_cookie_policy.h"
#include "chrome/browser/net/chrome_dns_cert_provenance_checker_factory.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/browser/resource_context.h"
#include "content/common/notification_service.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "webkit/database/database_tracker.h"

namespace {

// ----------------------------------------------------------------------------
// CookieMonster::Delegate implementation
// ----------------------------------------------------------------------------
class ChromeCookieMonsterDelegate : public net::CookieMonster::Delegate {
 public:
  explicit ChromeCookieMonsterDelegate(Profile* profile) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    profile_getter_ = new ProfileGetter(profile);
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
  // This class allows us to safely access the Profile pointer. The Delegate
  // itself cannot observe the PROFILE_DESTROYED notification, since it cannot
  // guarantee to be deleted on the UI thread and therefore unregister from
  // the notifications. All methods of ProfileGetter must be invoked on the UI
  // thread.
  class ProfileGetter
      : public base::RefCountedThreadSafe<ProfileGetter,
                                          BrowserThread::DeleteOnUIThread>,
        public NotificationObserver {
   public:
    explicit ProfileGetter(Profile* profile) : profile_(profile) {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      registrar_.Add(this,
                     NotificationType::PROFILE_DESTROYED,
                     Source<Profile>(profile_));
    }

    // NotificationObserver implementation.
    void Observe(NotificationType type,
                 const NotificationSource& source,
                 const NotificationDetails& details) {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      if (NotificationType::PROFILE_DESTROYED == type) {
        Profile* profile = Source<Profile>(source).ptr();
        if (profile_ == profile)
          profile_ = NULL;
      }
    }

    Profile* get() {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      return profile_;
    }

   private:
    friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
    friend class DeleteTask<ProfileGetter>;

    virtual ~ProfileGetter() {}

    NotificationRegistrar registrar_;

    Profile* profile_;
  };

  virtual ~ChromeCookieMonsterDelegate() {}

  void OnCookieChangedAsyncHelper(
      const net::CookieMonster::CanonicalCookie& cookie,
      bool removed,
      net::CookieMonster::Delegate::ChangeCause cause) {
    if (profile_getter_->get()) {
      ChromeCookieDetails cookie_details(&cookie, removed, cause);
      NotificationService::current()->Notify(
          NotificationType::COOKIE_CHANGED,
          Source<Profile>(profile_getter_->get()),
          Details<ChromeCookieDetails>(&cookie_details));
    }
  }

  scoped_refptr<ProfileGetter> profile_getter_;
};

}  // namespace

void ProfileIOData::InitializeProfileParams(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* pref_service = profile->GetPrefs();

  scoped_ptr<ProfileParams> params(new ProfileParams);
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
  params->transport_security_state = profile->GetTransportSecurityState();

  if (profile->GetUserScriptMaster()) {
    params->user_script_dir_path =
        profile->GetUserScriptMaster()->user_script_dir();
  }

  params->ssl_config_service = profile->GetSSLConfigService();
  params->cookie_monster_delegate = new ChromeCookieMonsterDelegate(profile);
  params->database_tracker = profile->GetDatabaseTracker();
  params->appcache_service = profile->GetAppCacheService();
  params->blob_storage_context = profile->GetBlobStorageContext();
  params->file_system_context = profile->GetFileSystemContext();
  params->extension_info_map = profile->GetExtensionInfoMap();
  params->prerender_manager = profile->GetPrerenderManager();
  params->protocol_handler_registry = profile->GetProtocolHandlerRegistry();

  params->proxy_config_service.reset(
      ProxyServiceFactory::CreateProxyConfigService(
          profile->GetProxyConfigTracker()));
  params->profile_id = profile->GetRuntimeId();
  profile_params_.reset(params.release());
}

ProfileIOData::RequestContext::RequestContext() {}
ProfileIOData::RequestContext::~RequestContext() {}

ProfileIOData::ProfileParams::ProfileParams()
    : is_incognito(false),
      clear_local_state_on_exit(false),
      profile_id(Profile::kInvalidProfileId) {}
ProfileIOData::ProfileParams::~ProfileParams() {}

ProfileIOData::ProfileIOData(bool is_incognito)
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(resource_context_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ProfileIOData::~ProfileIOData() {
  // If we have never initialized ProfileIOData, then Handle may hold the only
  // reference to it. The important thing is to make sure it hasn't been
  // initialized yet, because the lazily initialized variables are supposed to
  // live on the IO thread.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI))
    DCHECK(!initialized_);
  else
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetMainRequestContext() const {
  LazyInitialize();
  scoped_refptr<RequestContext> context = main_request_context_;
  context->set_profile_io_data(this);
  main_request_context_ = NULL;
  return context;
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
  scoped_refptr<RequestContext> context =
      extensions_request_context_;
  context->set_profile_io_data(this);
  extensions_request_context_ = NULL;
  return context;
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetIsolatedAppRequestContext(
    scoped_refptr<ChromeURLRequestContext> main_context,
    const std::string& app_id) const {
  LazyInitialize();
  scoped_refptr<ChromeURLRequestContext> context =
      AcquireIsolatedAppRequestContext(main_context, app_id);
  DCHECK(context);
  return context;
}

const content::ResourceContext& ProfileIOData::GetResourceContext() const {
  return resource_context_;
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
  main_request_context_ = new RequestContext;
  extensions_request_context_ = new RequestContext;

  profile_params_->appcache_service->set_request_context(main_request_context_);

  // Create objects pointed to by URLRequestContext.
  cookie_policy_.reset(
      new ChromeCookiePolicy(profile_params_->host_content_settings_map));

  network_delegate_.reset(new ChromeNetworkDelegate(
        io_thread_globals->extension_event_router_forwarder.get(),
        profile_params_->profile_id,
        &enable_referrers_,
        profile_params_->protocol_handler_registry));

  dns_cert_checker_.reset(
      CreateDnsCertProvenanceChecker(io_thread_globals->dnsrr_resolver.get(),
                                     main_request_context_));

  proxy_service_ =
      ProxyServiceFactory::CreateProxyService(
          io_thread->net_log(),
          io_thread_globals->proxy_script_fetcher_context.get(),
          profile_params_->proxy_config_service.release(),
          command_line);

  // Take ownership over these parameters.
  database_tracker_ = profile_params_->database_tracker;
  appcache_service_ = profile_params_->appcache_service;
  blob_storage_context_ = profile_params_->blob_storage_context;
  file_system_context_ = profile_params_->file_system_context;

  resource_context_.set_host_resolver(io_thread_globals->host_resolver.get());
  resource_context_.set_request_context(main_request_context_);
  resource_context_.set_database_tracker(database_tracker_);
  resource_context_.set_appcache_service(appcache_service_);
  resource_context_.set_blob_storage_context(blob_storage_context_);
  resource_context_.set_file_system_context(file_system_context_);

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
  context->set_user_script_dir_path(profile_params_->user_script_dir_path);
  context->set_host_content_settings_map(
      profile_params_->host_content_settings_map);
  context->set_host_zoom_map(profile_params_->host_zoom_map);
  context->set_transport_security_state(
      profile_params_->transport_security_state);
  context->set_ssl_config_service(profile_params_->ssl_config_service);
  context->set_appcache_service(profile_params_->appcache_service);
  context->set_blob_storage_context(profile_params_->blob_storage_context);
  context->set_file_system_context(profile_params_->file_system_context);
  context->set_extension_info_map(profile_params_->extension_info_map);
  context->set_prerender_manager(profile_params_->prerender_manager);
}

void ProfileIOData::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enable_referrers_.Destroy();
}

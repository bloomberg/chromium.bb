// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/libcros_service_library.h"
#include "chrome/browser/chromeos/proxy_config_service.h"
#endif  // defined(OS_CHROMEOS)

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
      bool removed) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &ChromeCookieMonsterDelegate::OnCookieChangedAsyncHelper,
            cookie,
            removed));
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
      bool removed) {
    if (profile_getter_->get()) {
      ChromeCookieDetails cookie_details(&cookie, removed);
      NotificationService::current()->Notify(
          NotificationType::COOKIE_CHANGED,
          Source<Profile>(profile_getter_->get()),
          Details<ChromeCookieDetails>(&cookie_details));
    }
  }

  scoped_refptr<ProfileGetter> profile_getter_;
};

}  // namespace

void ProfileIOData::InitializeProfileParams(Profile* profile,
                                            ProfileParams* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* pref_service = profile->GetPrefs();

  params->is_off_the_record = profile->IsOffTheRecord();
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

  params->proxy_config_service.reset(CreateProxyConfigService(profile));
  params->profile_id = profile->GetRuntimeId();
}

ProfileIOData::RequestContext::RequestContext() {}
ProfileIOData::RequestContext::~RequestContext() {}

ProfileIOData::ProfileParams::ProfileParams()
    : is_off_the_record(false),
      clear_local_state_on_exit(false),
      profile_id(Profile::kInvalidProfileId) {}
ProfileIOData::ProfileParams::~ProfileParams() {}

ProfileIOData::ProfileIOData(bool is_off_the_record)
    : initialized_(false) {
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
  scoped_refptr<ChromeURLRequestContext> context =
      AcquireMainRequestContext();
  DCHECK(context);
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
  scoped_refptr<ChromeURLRequestContext> context =
      AcquireExtensionsRequestContext();
  DCHECK(context);
  return context;
}

void ProfileIOData::LazyInitialize() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (initialized_)
    return;
  LazyInitializeInternal();
  initialized_ = true;
}

// static
void ProfileIOData::ApplyProfileParamsToContext(
    const ProfileParams& profile_params,
    ChromeURLRequestContext* context) {
  context->set_is_off_the_record(profile_params.is_off_the_record);
  context->set_accept_language(profile_params.accept_language);
  context->set_accept_charset(profile_params.accept_charset);
  context->set_referrer_charset(profile_params.referrer_charset);
  context->set_user_script_dir_path(profile_params.user_script_dir_path);
  context->set_host_content_settings_map(
      profile_params.host_content_settings_map);
  context->set_host_zoom_map(profile_params.host_zoom_map);
  context->set_transport_security_state(
      profile_params.transport_security_state);
  context->set_ssl_config_service(profile_params.ssl_config_service);
  context->set_database_tracker(profile_params.database_tracker);
  context->set_appcache_service(profile_params.appcache_service);
  context->set_blob_storage_context(profile_params.blob_storage_context);
  context->set_file_system_context(profile_params.file_system_context);
  context->set_extension_info_map(profile_params.extension_info_map);
  context->set_prerender_manager(profile_params.prerender_manager);
}

// static
net::ProxyConfigService* ProfileIOData::CreateProxyConfigService(
    Profile* profile) {
  // The linux gconf-based proxy settings getter relies on being initialized
  // from the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Create a baseline service that provides proxy configuration in case nothing
  // is configured through prefs (Note: prefs include command line and
  // configuration policy).
  net::ProxyConfigService* base_service = NULL;

  // TODO(port): the IO and FILE message loops are only used by Linux.  Can
  // that code be moved to chrome/browser instead of being in net, so that it
  // can use BrowserThread instead of raw MessageLoop pointers? See bug 25354.
#if defined(OS_CHROMEOS)
  base_service = new chromeos::ProxyConfigService(
      profile->GetChromeOSProxyConfigServiceImpl());
#else
  base_service = net::ProxyService::CreateSystemProxyConfigService(
      g_browser_process->io_thread()->message_loop(),
      g_browser_process->file_thread()->message_loop());
#endif  // defined(OS_CHROMEOS)

  return new PrefProxyConfigService(profile->GetProxyConfigTracker(),
                                    base_service);
}

// static
net::ProxyService* ProfileIOData::CreateProxyService(
    net::NetLog* net_log,
    net::URLRequestContext* context,
    net::ProxyConfigService* proxy_config_service,
    const CommandLine& command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool use_v8 = !command_line.HasSwitch(switches::kWinHttpProxyResolver);
  if (use_v8 && command_line.HasSwitch(switches::kSingleProcess)) {
    // See the note about V8 multithreading in net/proxy/proxy_resolver_v8.h
    // to understand why we have this limitation.
    LOG(ERROR) << "Cannot use V8 Proxy resolver in single process mode.";
    use_v8 = false;  // Fallback to non-v8 implementation.
  }

  size_t num_pac_threads = 0u;  // Use default number of threads.

  // Check the command line for an override on the number of proxy resolver
  // threads to use.
  if (command_line.HasSwitch(switches::kNumPacThreads)) {
    std::string s = command_line.GetSwitchValueASCII(switches::kNumPacThreads);

    // Parse the switch (it should be a positive integer formatted as decimal).
    int n;
    if (base::StringToInt(s, &n) && n > 0) {
      num_pac_threads = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for number of PAC threads: " << s;
    }
  }

  net::ProxyService* proxy_service;
  if (use_v8) {
    proxy_service = net::ProxyService::CreateUsingV8ProxyResolver(
        proxy_config_service,
        num_pac_threads,
        new net::ProxyScriptFetcherImpl(context),
        context->host_resolver(),
        net_log);
  } else {
    proxy_service = net::ProxyService::CreateUsingSystemProxyResolver(
        proxy_config_service,
        num_pac_threads,
        net_log);
  }

#if defined(OS_CHROMEOS)
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    chromeos::CrosLibrary::Get()->GetLibCrosServiceLibrary()->
        RegisterNetworkProxyHandler(proxy_service);
  }
#endif  // defined(OS_CHROMEOS)

  return proxy_service;
}

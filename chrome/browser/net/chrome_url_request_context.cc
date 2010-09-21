// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_url_request_context.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "net/base/static_cookie_policy.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/webkit_glue.h"

#if defined(USE_NSS)
#include "net/ocsp/nss_ocsp.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/proxy_config_service.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// ----------------------------------------------------------------------------
// Helper methods to check current thread
// ----------------------------------------------------------------------------

void CheckCurrentlyOnIOThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
}

void CheckCurrentlyOnMainThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

// ----------------------------------------------------------------------------
// Helper methods to initialize proxy
// ----------------------------------------------------------------------------

net::ProxyConfigService* CreateProxyConfigService(Profile* profile) {
  // The linux gconf-based proxy settings getter relies on being initialized
  // from the UI thread.
  CheckCurrentlyOnMainThread();

  scoped_ptr<net::ProxyConfig> proxy_config(CreateProxyConfig(
      profile->GetPrefs()));

  if (!proxy_config.get()) {
    // Use system settings.
    // TODO(port): the IO and FILE message loops are only used by Linux.  Can
    // that code be moved to chrome/browser instead of being in net, so that it
    // can use ChromeThread instead of raw MessageLoop pointers? See bug 25354.
#if defined(OS_CHROMEOS)
    return new chromeos::ProxyConfigService(
        profile->GetChromeOSProxyConfigServiceImpl());
#else
    return net::ProxyService::CreateSystemProxyConfigService(
        g_browser_process->io_thread()->message_loop(),
        g_browser_process->file_thread()->message_loop());
#endif  // defined(OS_CHROMEOS)
  }

  // Otherwise use the fixed settings from the command line.
  return new net::ProxyConfigServiceFixed(*proxy_config.get());
}

// Create a proxy service according to the options on command line.
net::ProxyService* CreateProxyService(
    net::NetLog* net_log,
    URLRequestContext* context,
    net::ProxyConfigService* proxy_config_service,
    const CommandLine& command_line,
    MessageLoop* io_loop) {
  CheckCurrentlyOnIOThread();

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

  return net::ProxyService::Create(
      proxy_config_service,
      use_v8,
      num_pac_threads,
      context,
      net_log,
      io_loop);
}

// ----------------------------------------------------------------------------
// CookieMonster::Delegate implementation
// ----------------------------------------------------------------------------
class ChromeCookieMonsterDelegate : public net::CookieMonster::Delegate {
 public:
  explicit ChromeCookieMonsterDelegate(Profile* profile) {
    CheckCurrentlyOnMainThread();
    profile_getter_ = new ProfileGetter(profile);
  }

  // net::CookieMonster::Delegate implementation.
  virtual void OnCookieChanged(
      const net::CookieMonster::CanonicalCookie& cookie,
      bool removed) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
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
                                          ChromeThread::DeleteOnUIThread>,
        public NotificationObserver {
   public:
    explicit ProfileGetter(Profile* profile) : profile_(profile) {
      CheckCurrentlyOnMainThread();
      registrar_.Add(this,
                     NotificationType::PROFILE_DESTROYED,
                     Source<Profile>(profile_));
    }

    // NotificationObserver implementation.
    void Observe(NotificationType type,
                 const NotificationSource& source,
                 const NotificationDetails& details) {
      CheckCurrentlyOnMainThread();
      if (NotificationType::PROFILE_DESTROYED == type) {
        Profile* profile = Source<Profile>(source).ptr();
        if (profile_ == profile)
          profile_ = NULL;
      }
    }

    Profile* get() {
      CheckCurrentlyOnMainThread();
      return profile_;
    }

   private:
    friend class ::ChromeThread;
    friend class DeleteTask<ProfileGetter>;

    virtual ~ProfileGetter() {}

    NotificationRegistrar registrar_;

    Profile* profile_;
  };

  virtual ~ChromeCookieMonsterDelegate() {}

  void OnCookieChangedAsyncHelper(
      net::CookieMonster::CanonicalCookie cookie,
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

// ----------------------------------------------------------------------------
// Helper factories
// ----------------------------------------------------------------------------

// Factory that creates the main ChromeURLRequestContext.
class FactoryForOriginal : public ChromeURLRequestContextFactory {
 public:
  FactoryForOriginal(Profile* profile,
                     const FilePath& cookie_store_path,
                     const FilePath& disk_cache_path,
                     int cache_size)
      : ChromeURLRequestContextFactory(profile),
        cookie_store_path_(cookie_store_path),
        disk_cache_path_(disk_cache_path),
        cache_size_(cache_size),
        // We need to initialize the ProxyConfigService from the UI thread
        // because on linux it relies on initializing things through gconf,
        // and needs to be on the main thread.
        proxy_config_service_(CreateProxyConfigService(profile)) {
  }

  virtual ChromeURLRequestContext* Create();

 private:
  FilePath cookie_store_path_;
  FilePath disk_cache_path_;
  int cache_size_;

  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
};

ChromeURLRequestContext* FactoryForOriginal::Create() {
  ChromeURLRequestContext* context = new ChromeURLRequestContext;
  ApplyProfileParametersToContext(context);

  IOThread::Globals* io_thread_globals = io_thread()->globals();

  // Global host resolver for the context.
  context->set_host_resolver(io_thread_globals->host_resolver);
  context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  context->set_proxy_service(
      CreateProxyService(io_thread_globals->net_log.get(),
                         context,
                         proxy_config_service_.release(),
                         command_line,
                         MessageLoop::current() /*io_loop*/));

  net::HttpCache::DefaultBackend* backend = new net::HttpCache::DefaultBackend(
      net::DISK_CACHE, disk_cache_path_, cache_size_,
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::CACHE));
  net::HttpCache* cache =
      new net::HttpCache(context->host_resolver(),
                         context->proxy_service(),
                         context->ssl_config_service(),
                         context->http_auth_handler_factory(),
                         &io_thread_globals->network_delegate,
                         io_thread_globals->net_log.get(),
                         backend);

  if (command_line.HasSwitch(switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  bool record_mode = chrome::kRecordModeEnabled &&
                     command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  if (record_mode || playback_mode) {
    // Don't use existing cookies and use an in-memory store.
    context->set_cookie_store(new net::CookieMonster(NULL,
        cookie_monster_delegate_));
    cache->set_mode(
        record_mode ? net::HttpCache::RECORD : net::HttpCache::PLAYBACK);
  }
  context->set_http_transaction_factory(cache);

  context->set_ftp_transaction_factory(
      new net::FtpNetworkLayer(context->host_resolver()));

  // setup cookie store
  if (!context->cookie_store()) {
    DCHECK(!cookie_store_path_.empty());

    scoped_refptr<SQLitePersistentCookieStore> cookie_db =
        new SQLitePersistentCookieStore(cookie_store_path_);
    context->set_cookie_store(new net::CookieMonster(cookie_db.get(),
        cookie_monster_delegate_));
  }

  context->set_cookie_policy(
      new ChromeCookiePolicy(host_content_settings_map_));

  appcache_service_->set_request_context(context);

#if defined(USE_NSS)
  // TODO(ukai): find a better way to set the URLRequestContext for OCSP.
  net::SetURLRequestContextForOCSP(context);
#endif

  context->set_net_log(io_thread_globals->net_log.get());
  return context;
}

// Factory that creates the ChromeURLRequestContext for extensions.
class FactoryForExtensions : public ChromeURLRequestContextFactory {
 public:
  FactoryForExtensions(Profile* profile, const FilePath& cookie_store_path,
                       bool incognito)
      : ChromeURLRequestContextFactory(profile),
        cookie_store_path_(cookie_store_path),
        incognito_(incognito) {
  }

  virtual ChromeURLRequestContext* Create();

 private:
  FilePath cookie_store_path_;
  bool incognito_;
};

ChromeURLRequestContext* FactoryForExtensions::Create() {
  ChromeURLRequestContext* context = new ChromeURLRequestContext;
  ApplyProfileParametersToContext(context);

  IOThread::Globals* io_thread_globals = io_thread()->globals();

  // All we care about for extensions is the cookie store. For incognito, we
  // use a non-persistent cookie store.
  scoped_refptr<SQLitePersistentCookieStore> cookie_db = NULL;
  if (!incognito_) {
    DCHECK(!cookie_store_path_.empty());
    cookie_db = new SQLitePersistentCookieStore(cookie_store_path_);
  }

  net::CookieMonster* cookie_monster =
      new net::CookieMonster(cookie_db.get(), NULL);

  // Enable cookies for extension URLs only.
  const char* schemes[] = {chrome::kExtensionScheme};
  cookie_monster->SetCookieableSchemes(schemes, 1);
  context->set_cookie_store(cookie_monster);
  // TODO(cbentzel): How should extensions handle HTTP Authentication?
  context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  return context;
}

// Factory that creates the ChromeURLRequestContext for incognito profile.
class FactoryForOffTheRecord : public ChromeURLRequestContextFactory {
 public:
  explicit FactoryForOffTheRecord(Profile* profile)
      : ChromeURLRequestContextFactory(profile),
        original_context_getter_(
            static_cast<ChromeURLRequestContextGetter*>(
                profile->GetOriginalProfile()->GetRequestContext())) {
  }

  virtual ChromeURLRequestContext* Create();

 private:
  scoped_refptr<ChromeURLRequestContextGetter> original_context_getter_;
};

ChromeURLRequestContext* FactoryForOffTheRecord::Create() {
  ChromeURLRequestContext* context = new ChromeURLRequestContext;
  ApplyProfileParametersToContext(context);

  ChromeURLRequestContext* original_context =
      original_context_getter_->GetIOContext();

  IOThread::Globals* io_thread_globals = io_thread()->globals();

  // Share the same proxy service, host resolver, and http_auth_handler_factory
  // as the original profile.
  context->set_host_resolver(original_context->host_resolver());
  context->set_proxy_service(original_context->proxy_service());
  context->set_http_auth_handler_factory(
      original_context->http_auth_handler_factory());

  net::HttpCache::BackendFactory* backend =
      net::HttpCache::DefaultBackend::InMemory(0);

  net::HttpCache* cache =
      new net::HttpCache(context->host_resolver(),
                         context->proxy_service(),
                         context->ssl_config_service(),
                         context->http_auth_handler_factory(),
                         &io_thread_globals->network_delegate,
                         io_thread_globals->net_log.get(),
                         backend);
  context->set_cookie_store(new net::CookieMonster(NULL,
      cookie_monster_delegate_));
  context->set_cookie_policy(
      new ChromeCookiePolicy(host_content_settings_map_));
  context->set_http_transaction_factory(cache);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  context->set_ftp_transaction_factory(
      new net::FtpNetworkLayer(context->host_resolver()));

  appcache_service_->set_request_context(context);

  context->set_net_log(io_thread_globals->net_log.get());
  return context;
}

// Factory that creates the ChromeURLRequestContext for media.
class FactoryForMedia : public ChromeURLRequestContextFactory {
 public:
  FactoryForMedia(Profile* profile,
                  const FilePath& disk_cache_path,
                  int cache_size,
                  bool off_the_record)
      : ChromeURLRequestContextFactory(profile),
        main_context_getter_(
            static_cast<ChromeURLRequestContextGetter*>(
                profile->GetRequestContext())),
        disk_cache_path_(disk_cache_path),
        cache_size_(cache_size) {
    is_media_ = true;
    is_off_the_record_ = off_the_record;
  }

  virtual ChromeURLRequestContext* Create();

 private:
  scoped_refptr<ChromeURLRequestContextGetter> main_context_getter_;

  FilePath disk_cache_path_;
  int cache_size_;
};

ChromeURLRequestContext* FactoryForMedia::Create() {
  ChromeURLRequestContext* context = new ChromeURLRequestContext;
  ApplyProfileParametersToContext(context);

  ChromeURLRequestContext* main_context =
      main_context_getter_->GetIOContext();

  IOThread::Globals* io_thread_globals = io_thread()->globals();

  // Share the same proxy service of the common profile.
  context->set_proxy_service(main_context->proxy_service());
  context->set_http_auth_handler_factory(
      main_context->http_auth_handler_factory());

  // Also share the cookie store of the common profile.
  context->set_cookie_store(main_context->cookie_store());
  context->set_cookie_policy(
      static_cast<ChromeCookiePolicy*>(main_context->cookie_policy()));

  // Create a media cache with default size.
  // TODO(hclam): make the maximum size of media cache configurable.
  net::HttpCache::DefaultBackend* backend = new net::HttpCache::DefaultBackend(
      net::MEDIA_CACHE, disk_cache_path_, cache_size_,
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::CACHE));

  net::HttpCache* main_cache =
      main_context->http_transaction_factory()->GetCache();
  net::HttpCache* cache;
  if (main_cache) {
    // Try to reuse HttpNetworkSession in the main context, assuming that
    // HttpTransactionFactory (network_layer()) of HttpCache is implemented
    // by HttpNetworkLayer so we can reuse HttpNetworkSession within it. This
    // assumption will be invalid if the original HttpCache is constructed with
    // HttpCache(HttpTransactionFactory*, BackendFactory*) constructor.
    net::HttpNetworkLayer* main_network_layer =
        static_cast<net::HttpNetworkLayer*>(main_cache->network_layer());
    cache = new net::HttpCache(main_network_layer->GetSession(), backend);
    // TODO(eroman): Since this is poaching the session from the main
    // context, it should hold a reference to that context preventing the
    // session from getting deleted.
  } else {
    // If original HttpCache doesn't exist, simply construct one with a whole
    // new set of network stack.
    cache = new net::HttpCache(main_context->host_resolver(),
                               main_context->proxy_service(),
                               main_context->ssl_config_service(),
                               main_context->http_auth_handler_factory(),
                               &io_thread_globals->network_delegate,
                               io_thread_globals->net_log.get(),
                               backend);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  context->set_http_transaction_factory(cache);
  context->set_net_log(io_thread_globals->net_log.get());

  return context;
}

}  // namespace

// ----------------------------------------------------------------------------
// ChromeURLRequestContextGetter
// ----------------------------------------------------------------------------

ChromeURLRequestContextGetter::ChromeURLRequestContextGetter(
    Profile* profile,
    ChromeURLRequestContextFactory* factory)
  : prefs_(NULL),
    factory_(factory),
    url_request_context_(NULL) {
  DCHECK(factory);

  // If a base profile was specified, listen for changes to the preferences.
  if (profile)
    RegisterPrefsObserver(profile);
}

ChromeURLRequestContextGetter::~ChromeURLRequestContextGetter() {
  CheckCurrentlyOnIOThread();

  DCHECK(!prefs_) << "Probably didn't call CleanupOnUIThread";

  // Either we already transformed the factory into a URLRequestContext, or
  // we still have a pending factory.
  DCHECK((factory_.get() && !url_request_context_.get()) ||
         (!factory_.get() && url_request_context_.get()));

  // The scoped_refptr / scoped_ptr destructors take care of releasing
  // |factory_| and |url_request_context_| now.
}

// Lazily create a ChromeURLRequestContext using our factory.
URLRequestContext* ChromeURLRequestContextGetter::GetURLRequestContext() {
  CheckCurrentlyOnIOThread();

  if (!url_request_context_) {
    DCHECK(factory_.get());
    url_request_context_ = factory_->Create();
    factory_.reset();
  }

  return url_request_context_;
}

void ChromeURLRequestContextGetter::RegisterUserPrefs(
    PrefService* pref_service) {
  pref_service->RegisterBooleanPref(prefs::kNoProxyServer, false);
  pref_service->RegisterBooleanPref(prefs::kProxyAutoDetect, false);
  pref_service->RegisterStringPref(prefs::kProxyServer, "");
  pref_service->RegisterStringPref(prefs::kProxyPacUrl, "");
  pref_service->RegisterStringPref(prefs::kProxyBypassList, "");
}

net::CookieStore* ChromeURLRequestContextGetter::GetCookieStore() {
  // If we are running on the IO thread this is real easy.
  if (ChromeThread::CurrentlyOn(ChromeThread::IO))
    return GetURLRequestContext()->cookie_store();

  // If we aren't running on the IO thread, we cannot call
  // GetURLRequestContext(). Instead we will post a task to the IO loop
  // and wait for it to complete.

  base::WaitableEvent completion(false, false);
  net::CookieStore* result = NULL;

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
          &ChromeURLRequestContextGetter::GetCookieStoreAsyncHelper,
          &completion,
          &result));

  completion.Wait();
  DCHECK(result);
  return result;
}

scoped_refptr<base::MessageLoopProxy>
ChromeURLRequestContextGetter::GetIOMessageLoopProxy() {
  return ChromeThread::GetMessageLoopProxyForThread(ChromeThread::IO);
}

// static
ChromeURLRequestContextGetter* ChromeURLRequestContextGetter::CreateOriginal(
    Profile* profile, const FilePath& cookie_store_path,
    const FilePath& disk_cache_path, int cache_size) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile,
      new FactoryForOriginal(profile,
                             cookie_store_path,
                             disk_cache_path,
                             cache_size));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOriginalForMedia(
    Profile* profile, const FilePath& disk_cache_path, int cache_size) {
  DCHECK(!profile->IsOffTheRecord());
  return CreateRequestContextForMedia(profile, disk_cache_path, cache_size,
                                      false);
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOriginalForExtensions(
    Profile* profile, const FilePath& cookie_store_path) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile,
      new FactoryForExtensions(profile, cookie_store_path, false));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOffTheRecord(Profile* profile) {
  DCHECK(profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile, new FactoryForOffTheRecord(profile));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOffTheRecordForExtensions(
    Profile* profile) {
  DCHECK(profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile, new FactoryForExtensions(profile, FilePath(), true));
}

void ChromeURLRequestContextGetter::CleanupOnUIThread() {
  CheckCurrentlyOnMainThread();

  if (prefs_) {
    // Unregister for pref notifications.
    prefs_->RemovePrefObserver(prefs::kAcceptLanguages, this);
    prefs_->RemovePrefObserver(prefs::kDefaultCharset, this);
    prefs_ = NULL;
  }
}

void ChromeURLRequestContextGetter::OnNewExtensions(
    const std::string& id,
    ChromeURLRequestContext::ExtensionInfo* info) {
  GetIOContext()->OnNewExtensions(id, info);
}

void ChromeURLRequestContextGetter::OnUnloadedExtension(
    const std::string& id) {
  GetIOContext()->OnUnloadedExtension(id);
}

// NotificationObserver implementation.
void ChromeURLRequestContextGetter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  CheckCurrentlyOnMainThread();

  if (NotificationType::PREF_CHANGED == type) {
    std::string* pref_name_in = Details<std::string>(details).ptr();
    PrefService* prefs = Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kAcceptLanguages) {
      std::string accept_language =
          prefs->GetString(prefs::kAcceptLanguages);
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(
              this,
              &ChromeURLRequestContextGetter::OnAcceptLanguageChange,
              accept_language));
    } else if (*pref_name_in == prefs::kDefaultCharset) {
      std::string default_charset =
          prefs->GetString(prefs::kDefaultCharset);
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(
              this,
              &ChromeURLRequestContextGetter::OnDefaultCharsetChange,
              default_charset));
    }
  } else {
    NOTREACHED();
  }
}

void ChromeURLRequestContextGetter::RegisterPrefsObserver(Profile* profile) {
  CheckCurrentlyOnMainThread();

  prefs_ = profile->GetPrefs();

  prefs_->AddPrefObserver(prefs::kAcceptLanguages, this);
  prefs_->AddPrefObserver(prefs::kDefaultCharset, this);
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateRequestContextForMedia(
    Profile* profile, const FilePath& disk_cache_path, int cache_size,
    bool off_the_record) {
  return new ChromeURLRequestContextGetter(
      profile,
      new FactoryForMedia(profile,
                          disk_cache_path,
                          cache_size,
                          off_the_record));
}

void ChromeURLRequestContextGetter::OnAcceptLanguageChange(
    const std::string& accept_language) {
  GetIOContext()->OnAcceptLanguageChange(accept_language);
}

void ChromeURLRequestContextGetter::OnDefaultCharsetChange(
    const std::string& default_charset) {
  GetIOContext()->OnDefaultCharsetChange(default_charset);
}

void ChromeURLRequestContextGetter::GetCookieStoreAsyncHelper(
    base::WaitableEvent* completion,
    net::CookieStore** result) {
  // Note that CookieStore is refcounted, yet we do not add a reference.
  *result = GetURLRequestContext()->cookie_store();
  completion->Signal();
}

// ----------------------------------------------------------------------------
// ChromeURLRequestContext
// ----------------------------------------------------------------------------

ChromeURLRequestContext::ChromeURLRequestContext()
    : is_media_(false),
      is_off_the_record_(false) {
  CheckCurrentlyOnIOThread();
}

ChromeURLRequestContext::~ChromeURLRequestContext() {
  CheckCurrentlyOnIOThread();

  if (appcache_service_.get() && appcache_service_->request_context() == this)
    appcache_service_->set_request_context(NULL);

  if (proxy_service_ &&
      proxy_service_->GetProxyScriptFetcher() &&
      proxy_service_->GetProxyScriptFetcher()->GetRequestContext() == this) {
    // Remove the ProxyScriptFetcher's weak reference to this context.
    proxy_service_->SetProxyScriptFetcher(NULL);
  }

#if defined(USE_NSS)
  if (this == net::GetURLRequestContextForOCSP()) {
    // We are releasing the URLRequestContext used by OCSP handlers.
    net::SetURLRequestContextForOCSP(NULL);
  }
#endif

  NotificationService::current()->Notify(
      NotificationType::URL_REQUEST_CONTEXT_RELEASED,
      Source<URLRequestContext>(this),
      NotificationService::NoDetails());

  delete ftp_transaction_factory_;
  delete http_transaction_factory_;

  // cookie_policy_'s lifetime is auto-managed by chrome_cookie_policy_.  We
  // null this out here to avoid a dangling reference to chrome_cookie_policy_
  // when ~URLRequestContext runs.
  cookie_policy_ = NULL;
}

std::string ChromeURLRequestContext::GetNameForExtension(
    const std::string& id) {
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  if (iter != extension_info_.end())
    return iter->second->name;
  else
    return std::string();
}

FilePath ChromeURLRequestContext::GetPathForExtension(const std::string& id) {
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  if (iter != extension_info_.end())
    return iter->second->path;
  else
    return FilePath();
}

bool ChromeURLRequestContext::ExtensionHasWebExtent(const std::string& id) {
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  return iter != extension_info_.end() && !iter->second->extent.is_empty();
}

bool ChromeURLRequestContext::ExtensionCanLoadInIncognito(
    const std::string& id) {
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  // Only split-mode extensions can load in incognito profiles.
  return iter != extension_info_.end() && iter->second->incognito_split_mode;
}

std::string ChromeURLRequestContext::GetDefaultLocaleForExtension(
    const std::string& id) {
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  std::string result;
  if (iter != extension_info_.end())
    result = iter->second->default_locale;

  return result;
}

ExtensionExtent
    ChromeURLRequestContext::GetEffectiveHostPermissionsForExtension(
        const std::string& id) {
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  ExtensionExtent result;
  if (iter != extension_info_.end())
    result = iter->second->effective_host_permissions;

  return result;
}

bool ChromeURLRequestContext::CheckURLAccessToExtensionPermission(
    const GURL& url,
    const char* permission_name) {
  ExtensionInfoMap::iterator info;
  if (url.SchemeIs(chrome::kExtensionScheme)) {
    // If the url is an extension scheme, we just look it up by extension id.
    std::string id = url.host();
    info = extension_info_.find(id);
  } else {
    // Otherwise, we scan for a matching extent. Overlapping extents are
    // disallowed, so only one will match.
    info = extension_info_.begin();
    while (info != extension_info_.end() &&
           !info->second->extent.ContainsURL(url))
      ++info;
  }

  if (info == extension_info_.end())
    return false;

  std::vector<std::string>& api_permissions = info->second->api_permissions;
  return std::find(api_permissions.begin(),
                   api_permissions.end(),
                   permission_name) != api_permissions.end();
}

const std::string& ChromeURLRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}

void ChromeURLRequestContext::OnNewExtensions(const std::string& id,
                                              ExtensionInfo* info) {
  extension_info_[id] = linked_ptr<ExtensionInfo>(info);
}

void ChromeURLRequestContext::OnUnloadedExtension(const std::string& id) {
  CheckCurrentlyOnIOThread();
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  if (iter != extension_info_.end()) {
    extension_info_.erase(iter);
  } else {
    // NOTE: This can currently happen if we receive multiple unload
    // notifications, e.g. setting incognito-enabled state for a
    // disabled extension (e.g., via sync).  See
    // http://code.google.com/p/chromium/issues/detail?id=50582 .
    NOTREACHED() << id;
  }
}

ChromeURLRequestContext::ChromeURLRequestContext(
    ChromeURLRequestContext* other) {
  CheckCurrentlyOnIOThread();

  // Set URLRequestContext members
  net_log_ = other->net_log_;
  host_resolver_ = other->host_resolver_;
  proxy_service_ = other->proxy_service_;
  ssl_config_service_ = other->ssl_config_service_;
  http_transaction_factory_ = other->http_transaction_factory_;
  ftp_transaction_factory_ = other->ftp_transaction_factory_;
  cookie_store_ = other->cookie_store_;
  cookie_policy_ = other->cookie_policy_;
  transport_security_state_ = other->transport_security_state_;
  accept_language_ = other->accept_language_;
  accept_charset_ = other->accept_charset_;
  referrer_charset_ = other->referrer_charset_;
  // NOTE(cbentzel): Sharing the http_auth_handler_factory_ is potentially
  // dangerous because it is a raw pointer. However, the current implementation
  // creates and destroys the pointed-to-object in the io_thread and it is
  // valid for the lifetime of all ChromeURLRequestContext objects.
  // If this is no longer the case, HttpAuthHandlerFactory will need to be
  // ref-counted or cloneable.
  http_auth_handler_factory_ = other->http_auth_handler_factory_;

  // Set ChromeURLRequestContext members
  extension_info_ = other->extension_info_;
  user_script_dir_path_ = other->user_script_dir_path_;
  appcache_service_ = other->appcache_service_;
  database_tracker_ = other->database_tracker_;
  chrome_cookie_policy_ = other->chrome_cookie_policy_;
  host_content_settings_map_ = other->host_content_settings_map_;
  host_zoom_map_ = other->host_zoom_map_;
  is_media_ = other->is_media_;
  is_off_the_record_ = other->is_off_the_record_;
  blob_storage_context_ = other->blob_storage_context_;
}

void ChromeURLRequestContext::OnAcceptLanguageChange(
    const std::string& accept_language) {
  CheckCurrentlyOnIOThread();
  accept_language_ =
      net::HttpUtil::GenerateAcceptLanguageHeader(accept_language);
}

void ChromeURLRequestContext::OnDefaultCharsetChange(
    const std::string& default_charset) {
  CheckCurrentlyOnIOThread();
  referrer_charset_ = default_charset;
  accept_charset_ =
      net::HttpUtil::GenerateAcceptCharsetHeader(default_charset);
}

// ----------------------------------------------------------------------------
// ChromeURLRequestContextFactory
// ----------------------------------------------------------------------------

// Extract values from |profile| and copy them into
// ChromeURLRequestContextFactory. We will use them later when constructing the
// ChromeURLRequestContext on the IO thread (see
// ApplyProfileParametersToContext() which reverses this).
ChromeURLRequestContextFactory::ChromeURLRequestContextFactory(Profile* profile)
    : is_media_(false),
      is_off_the_record_(profile->IsOffTheRecord()),
      io_thread_(g_browser_process->io_thread()) {
  CheckCurrentlyOnMainThread();
  PrefService* prefs = profile->GetPrefs();

  // Set up Accept-Language and Accept-Charset header values
  accept_language_ = net::HttpUtil::GenerateAcceptLanguageHeader(
      prefs->GetString(prefs::kAcceptLanguages));
  std::string default_charset = prefs->GetString(prefs::kDefaultCharset);
  accept_charset_ =
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
  referrer_charset_ = default_charset;

  host_content_settings_map_ = profile->GetHostContentSettingsMap();
  host_zoom_map_ = profile->GetHostZoomMap();
  transport_security_state_ = profile->GetTransportSecurityState();

  if (profile->GetExtensionsService()) {
    const ExtensionList* extensions =
        profile->GetExtensionsService()->extensions();
    for (ExtensionList::const_iterator iter = extensions->begin();
        iter != extensions->end(); ++iter) {
      extension_info_[(*iter)->id()] =
          linked_ptr<ChromeURLRequestContext::ExtensionInfo>(
              new ChromeURLRequestContext::ExtensionInfo(
                  (*iter)->name(),
                  (*iter)->path(),
                  (*iter)->default_locale(),
                  (*iter)->incognito_split_mode(),
                  (*iter)->web_extent(),
                  (*iter)->GetEffectiveHostPermissions(),
                  (*iter)->api_permissions()));
    }
  }

  if (profile->GetUserScriptMaster())
    user_script_dir_path_ = profile->GetUserScriptMaster()->user_script_dir();

  ssl_config_service_ = profile->GetSSLConfigService();
  profile_dir_path_ = profile->GetPath();
  cookie_monster_delegate_ = new ChromeCookieMonsterDelegate(profile);
  appcache_service_ = profile->GetAppCacheService();
  database_tracker_ = profile->GetDatabaseTracker();
  blob_storage_context_ = profile->GetBlobStorageContext();
}

ChromeURLRequestContextFactory::~ChromeURLRequestContextFactory() {
  CheckCurrentlyOnIOThread();
}

void ChromeURLRequestContextFactory::ApplyProfileParametersToContext(
    ChromeURLRequestContext* context) {
  // Apply all the parameters. NOTE: keep this in sync with
  // ChromeURLRequestContextFactory(Profile*).
  context->set_is_media(is_media_);
  context->set_is_off_the_record(is_off_the_record_);
  context->set_accept_language(accept_language_);
  context->set_accept_charset(accept_charset_);
  context->set_referrer_charset(referrer_charset_);
  context->set_extension_info(extension_info_);
  context->set_user_script_dir_path(user_script_dir_path_);
  context->set_host_content_settings_map(host_content_settings_map_);
  context->set_host_zoom_map(host_zoom_map_);
  context->set_transport_security_state(
      transport_security_state_);
  context->set_ssl_config_service(ssl_config_service_);
  context->set_appcache_service(appcache_service_);
  context->set_database_tracker(database_tracker_);
  context->set_blob_storage_context(blob_storage_context_);
}

// ----------------------------------------------------------------------------

net::ProxyConfig* CreateProxyConfig(const PrefService* pref_service) {
  // Scan for all "enable" type proxy switches.
  static const char* proxy_prefs[] = {
    prefs::kProxyPacUrl,
    prefs::kProxyServer,
    prefs::kProxyBypassList,
    prefs::kProxyAutoDetect
  };

  bool found_enable_proxy_pref = false;
  for (size_t i = 0; i < arraysize(proxy_prefs); i++) {
    if (pref_service->HasPrefPath(proxy_prefs[i])) {
      found_enable_proxy_pref = true;
      break;
    }
  }

  if (!found_enable_proxy_pref &&
      !pref_service->GetBoolean(prefs::kNoProxyServer)) {
    return NULL;
  }

  net::ProxyConfig* proxy_config = new net::ProxyConfig();
  if (pref_service->GetBoolean(prefs::kNoProxyServer)) {
    // Ignore all the other proxy config preferences if the use of a proxy
    // has been explicitly disabled.
    return proxy_config;
  }

  if (pref_service->HasPrefPath(prefs::kProxyServer)) {
    std::string proxy_server = pref_service->GetString(prefs::kProxyServer);
    proxy_config->proxy_rules().ParseFromString(proxy_server);
  }

  if (pref_service->HasPrefPath(prefs::kProxyPacUrl)) {
    std::string proxy_pac = pref_service->GetString(prefs::kProxyPacUrl);
    proxy_config->set_pac_url(GURL(proxy_pac));
  }

  proxy_config->set_auto_detect(pref_service->GetBoolean(
      prefs::kProxyAutoDetect));

  if (pref_service->HasPrefPath(prefs::kProxyBypassList)) {
    std::string proxy_bypass =
        pref_service->GetString(prefs::kProxyBypassList);
    proxy_config->proxy_rules().bypass_rules.ParseFromString(proxy_bypass);
  }

  return proxy_config;
}

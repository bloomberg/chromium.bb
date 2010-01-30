// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_url_request_context.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_request_info.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_LINUX)
#include "net/ocsp/nss_ocsp.h"
#endif

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

net::ProxyConfigService* CreateProxyConfigService(
    const CommandLine& command_line) {
  // The linux gconf-based proxy settings getter relies on being initialized
  // from the UI thread.
  CheckCurrentlyOnMainThread();

  scoped_ptr<net::ProxyConfig> proxy_config_from_cmd_line(
      CreateProxyConfig(command_line));

  if (!proxy_config_from_cmd_line.get()) {
    // Use system settings.
    // TODO(port): the IO and FILE message loops are only used by Linux.  Can
    // that code be moved to chrome/browser instead of being in net, so that it
    // can use ChromeThread instead of raw MessageLoop pointers? See bug 25354.
    return net::ProxyService::CreateSystemProxyConfigService(
        g_browser_process->io_thread()->message_loop(),
        g_browser_process->file_thread()->message_loop());
  }

  // Otherwise use the fixed settings from the command line.
  return new net::ProxyConfigServiceFixed(*proxy_config_from_cmd_line.get());
}

// Create a proxy service according to the options on command line.
net::ProxyService* CreateProxyService(
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

  return net::ProxyService::Create(
      proxy_config_service,
      use_v8,
      context,
      NULL, // TODO(eroman): Pass a valid NetworkChangeNotifier implementation
            //               (http://crbug.com/12293).
      io_loop);
}

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
        proxy_config_service_(
            CreateProxyConfigService(
                *CommandLine::ForCurrentProcess())) {
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

  // Global host resolver for the context.
  context->set_host_resolver(io_thread()->host_resolver());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  context->set_proxy_service(
      CreateProxyService(context,
                         proxy_config_service_.release(),
                         command_line,
                         MessageLoop::current() /*io_loop*/));

  net::HttpCache* cache =
      new net::HttpCache(context->host_resolver(),
                         context->proxy_service(),
                         context->ssl_config_service(),
                         disk_cache_path_, cache_size_);

  if (command_line.HasSwitch(switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  bool record_mode = chrome::kRecordModeEnabled &&
                     command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  if (record_mode || playback_mode) {
    // Don't use existing cookies and use an in-memory store.
    context->set_cookie_store(new net::CookieMonster());
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
    context->set_cookie_store(new net::CookieMonster(cookie_db.get()));
  }

  // Create a new AppCacheService (issues fetches through the
  // main URLRequestContext that we just created).
  context->set_appcache_service(
      new ChromeAppCacheService(profile_dir_path_, false));
  context->appcache_service()->set_request_context(context);

#if defined(OS_LINUX)
  // TODO(ukai): find a better way to set the URLRequestContext for OCSP.
  net::SetURLRequestContextForOCSP(context);
#endif

  return context;
}

// Factory that creates the ChromeURLRequestContext for extensions.
class FactoryForExtensions : public ChromeURLRequestContextFactory {
 public:
  FactoryForExtensions(Profile* profile, const FilePath& cookie_store_path)
      : ChromeURLRequestContextFactory(profile),
        cookie_store_path_(cookie_store_path) {
  }

  virtual ChromeURLRequestContext* Create();

 private:
  FilePath cookie_store_path_;
};

ChromeURLRequestContext* FactoryForExtensions::Create() {
  ChromeURLRequestContext* context = new ChromeURLRequestContext;
  ApplyProfileParametersToContext(context);

  // All we care about for extensions is the cookie store.
  DCHECK(!cookie_store_path_.empty());

  scoped_refptr<SQLitePersistentCookieStore> cookie_db =
      new SQLitePersistentCookieStore(cookie_store_path_);
  net::CookieMonster* cookie_monster = new net::CookieMonster(cookie_db.get());

  // Enable cookies for extension URLs only.
  const char* schemes[] = {chrome::kExtensionScheme};
  cookie_monster->SetCookieableSchemes(schemes, 1);
  context->set_cookie_store(cookie_monster);

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

  // Share the same proxy service and host resolver as the original profile.
  context->set_host_resolver(original_context->host_resolver());
  context->set_proxy_service(original_context->proxy_service());

  net::HttpCache* cache =
      new net::HttpCache(context->host_resolver(), context->proxy_service(),
                         context->ssl_config_service(), 0);
  context->set_cookie_store(new net::CookieMonster);
  context->set_http_transaction_factory(cache);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  context->set_ftp_transaction_factory(
      new net::FtpNetworkLayer(context->host_resolver()));

  // Create a separate AppCacheService for OTR mode.
  context->set_appcache_service(
      new ChromeAppCacheService(profile_dir_path_, true));
  context->appcache_service()->set_request_context(context);

  return context;
}

// Factory that creates the ChromeURLRequestContext for extensions in incognito
// mode.
class FactoryForOffTheRecordExtensions
    : public ChromeURLRequestContextFactory {
 public:
  explicit FactoryForOffTheRecordExtensions(Profile* profile)
      : ChromeURLRequestContextFactory(profile) {}

  virtual ChromeURLRequestContext* Create();
};

ChromeURLRequestContext* FactoryForOffTheRecordExtensions::Create() {
  ChromeURLRequestContext* context = new ChromeURLRequestContext;
  ApplyProfileParametersToContext(context);

  net::CookieMonster* cookie_monster = new net::CookieMonster;

  // Enable cookies for extension URLs only.
  const char* schemes[] = {chrome::kExtensionScheme};
  cookie_monster->SetCookieableSchemes(schemes, 1);
  context->set_cookie_store(cookie_monster);

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

  // Share the same proxy service of the common profile.
  context->set_proxy_service(main_context->proxy_service());
  // Also share the cookie store of the common profile.
  context->set_cookie_store(main_context->cookie_store());

  // Create a media cache with default size.
  // TODO(hclam): make the maximum size of media cache configurable.
  net::HttpCache* main_cache =
      main_context->http_transaction_factory()->GetCache();
  net::HttpCache* cache;
  if (main_cache) {
    // Try to reuse HttpNetworkSession in the main context, assuming that
    // HttpTransactionFactory (network_layer()) of HttpCache is implemented
    // by HttpNetworkLayer so we can reuse HttpNetworkSession within it. This
    // assumption will be invalid if the original HttpCache is constructed with
    // HttpCache(HttpTransactionFactory*, disk_cache::Backend*) constructor.
    net::HttpNetworkLayer* main_network_layer =
        static_cast<net::HttpNetworkLayer*>(main_cache->network_layer());
    cache = new net::HttpCache(main_network_layer->GetSession(),
                               disk_cache_path_, cache_size_);
    // TODO(eroman): Since this is poaching the session from the main
    // context, it should hold a reference to that context preventing the
    // session from getting deleted.
  } else {
    // If original HttpCache doesn't exist, simply construct one with a whole
    // new set of network stack.
    cache = new net::HttpCache(main_context->host_resolver(),
                               main_context->proxy_service(),
                               main_context->ssl_config_service(),
                               disk_cache_path_, cache_size_);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  cache->set_type(net::MEDIA_CACHE);
  context->set_http_transaction_factory(cache);

  // Use the same appcache service as the profile's main context.
  context->set_appcache_service(main_context->appcache_service());

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
      new FactoryForExtensions(profile, cookie_store_path));
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
      profile,
      new FactoryForOffTheRecordExtensions(profile));
}

void ChromeURLRequestContextGetter::CleanupOnUIThread() {
  CheckCurrentlyOnMainThread();

  if (prefs_) {
    // Unregister for pref notifications.
    prefs_->RemovePrefObserver(prefs::kAcceptLanguages, this);
    prefs_->RemovePrefObserver(prefs::kCookieBehavior, this);
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
    std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
    PrefService* prefs = Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kAcceptLanguages) {
      std::string accept_language =
          WideToASCII(prefs->GetString(prefs::kAcceptLanguages));
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(
              this,
              &ChromeURLRequestContextGetter::OnAcceptLanguageChange,
              accept_language));
    } else if (*pref_name_in == prefs::kCookieBehavior) {
      net::CookiePolicy::Type policy_type = net::CookiePolicy::FromInt(
          prefs_->GetInteger(prefs::kCookieBehavior));
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(
              this,
              &ChromeURLRequestContextGetter::OnCookiePolicyChange,
              policy_type));
    } else if (*pref_name_in == prefs::kDefaultCharset) {
      std::string default_charset =
          WideToASCII(prefs->GetString(prefs::kDefaultCharset));
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
  prefs_->AddPrefObserver(prefs::kCookieBehavior, this);
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

void ChromeURLRequestContextGetter::OnCookiePolicyChange(
    net::CookiePolicy::Type type) {
  GetIOContext()->OnCookiePolicyChange(type);
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

ChromeURLRequestContext::ChromeURLRequestContext() {
  CheckCurrentlyOnIOThread();
  url_request_tracker()->SetGraveyardFilter(
      &ChromeURLRequestContext::ShouldTrackRequest);
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

#if defined(OS_LINUX)
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
}

FilePath ChromeURLRequestContext::GetPathForExtension(const std::string& id) {
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  if (iter != extension_info_.end())
    return iter->second->path;
  else
    return FilePath();
}

std::string ChromeURLRequestContext::GetDefaultLocaleForExtension(
    const std::string& id) {
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  std::string result;
  if (iter != extension_info_.end())
    result = iter->second->default_locale;

  return result;
}

bool ChromeURLRequestContext::CheckURLAccessToExtensionPermission(
    const GURL& url,
    const std::string& application_id,
    const char* permission_name) {
  DCHECK(!application_id.empty());

  // Get the information about the specified extension. If the extension isn't
  // installed, then permission is not granted.
  ExtensionInfoMap::iterator info = extension_info_.find(application_id);
  if (info == extension_info_.end())
    return false;

  // Check that the extension declares the required permission.
  std::vector<std::string>& permissions = info->second->api_permissions;
  if (permissions.end() == std::find(permissions.begin(), permissions.end(),
                                     permission_name)) {
    return false;
  }

  // Check that the extension declares the source URL in its extent.
  Extension::URLPatternList& extent = info->second->extent;
  for (Extension::URLPatternList::iterator pattern = extent.begin();
       pattern != extent.end(); ++pattern) {
    if (pattern->MatchesUrl(url))
      return true;
  }

  return false;
}

const std::string& ChromeURLRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}

bool ChromeURLRequestContext::InterceptRequestCookies(
    const URLRequest* request, const std::string& cookies) const {
  return InterceptCookie(request, cookies);
}

bool ChromeURLRequestContext::InterceptResponseCookie(
    const URLRequest* request, const std::string& cookie) const {
  return InterceptCookie(request, cookie);
}

bool ChromeURLRequestContext::InterceptCookie(
    const URLRequest* request, const std::string& cookie) const {
  BlacklistRequestInfo* request_info =
      BlacklistRequestInfo::FromURLRequest(request);
  // Requests which don't go through ResourceDispatcherHost don't have privacy
  // blacklist request data.
  if (!request_info)
    return true;
  const Blacklist* blacklist = request_info->GetBlacklist();
  // TODO(phajdan.jr): remove the NULL check when blacklists are stable.
  if (!blacklist)
    return true;
  scoped_ptr<Blacklist::Match> match(blacklist->FindMatch(request->url()));
  if (match.get() && (match->attributes() & Blacklist::kBlockCookies)) {
    NotificationService::current()->Notify(
        NotificationType::BLACKLIST_NONVISUAL_RESOURCE_BLOCKED,
        Source<const ChromeURLRequestContext>(this),
        Details<const URLRequest>(request));
    return false;
  }

  return true;
}

const Blacklist* ChromeURLRequestContext::GetPrivacyBlacklist() const {
  return privacy_blacklist_.get();
}

void ChromeURLRequestContext::OnNewExtensions(const std::string& id,
                                              ExtensionInfo* info) {
  if (!is_off_the_record_)
    extension_info_[id] = linked_ptr<ExtensionInfo>(info);
}

void ChromeURLRequestContext::OnUnloadedExtension(const std::string& id) {
  CheckCurrentlyOnIOThread();
  if (is_off_the_record_)
    return;
  ExtensionInfoMap::iterator iter = extension_info_.find(id);
  DCHECK(iter != extension_info_.end());
  extension_info_.erase(iter);
}

ChromeURLRequestContext::ChromeURLRequestContext(
    ChromeURLRequestContext* other) {
  CheckCurrentlyOnIOThread();

  // Set URLRequestContext members
  host_resolver_ = other->host_resolver_;
  proxy_service_ = other->proxy_service_;
  ssl_config_service_ = other->ssl_config_service_;
  http_transaction_factory_ = other->http_transaction_factory_;
  ftp_transaction_factory_ = other->ftp_transaction_factory_;
  cookie_store_ = other->cookie_store_;
  cookie_policy_.set_type(other->cookie_policy_.type());
  transport_security_state_ = other->transport_security_state_;
  accept_language_ = other->accept_language_;
  accept_charset_ = other->accept_charset_;
  referrer_charset_ = other->referrer_charset_;

  // Set ChromeURLRequestContext members
  extension_info_ = other->extension_info_;
  user_script_dir_path_ = other->user_script_dir_path_;
  appcache_service_ = other->appcache_service_;
  host_content_settings_map_ = other->host_content_settings_map_;
  host_zoom_map_ = other->host_zoom_map_;
  privacy_blacklist_ = other->privacy_blacklist_;
  is_media_ = other->is_media_;
  is_off_the_record_ = other->is_off_the_record_;
}

void ChromeURLRequestContext::OnAcceptLanguageChange(
    const std::string& accept_language) {
  CheckCurrentlyOnIOThread();
  accept_language_ =
      net::HttpUtil::GenerateAcceptLanguageHeader(accept_language);
}

void ChromeURLRequestContext::OnCookiePolicyChange(
    net::CookiePolicy::Type type) {
  CheckCurrentlyOnIOThread();
  cookie_policy_.set_type(type);
}

void ChromeURLRequestContext::OnDefaultCharsetChange(
    const std::string& default_charset) {
  CheckCurrentlyOnIOThread();
  referrer_charset_ = default_charset;
  accept_charset_ =
      net::HttpUtil::GenerateAcceptCharsetHeader(default_charset);
}

// static
bool ChromeURLRequestContext::ShouldTrackRequest(const GURL& url) {
  // Exclude "chrome://" URLs from our recent requests circular buffer.
  return !url.SchemeIs("chrome");
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
      WideToASCII(prefs->GetString(prefs::kAcceptLanguages)));
  std::string default_charset =
      WideToASCII(prefs->GetString(prefs::kDefaultCharset));
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

  cookie_policy_type_ = net::CookiePolicy::FromInt(
      prefs->GetInteger(prefs::kCookieBehavior));

  host_content_settings_map_ = profile->GetHostContentSettingsMap();

  host_zoom_map_ = profile->GetHostZoomMap();

  privacy_blacklist_ = profile->GetPrivacyBlacklist();

  // TODO(eroman): this doesn't look safe; sharing between IO and UI threads!
  transport_security_state_ = profile->GetTransportSecurityState();

  if (profile->GetExtensionsService()) {
    const ExtensionList* extensions =
        profile->GetExtensionsService()->extensions();
    for (ExtensionList::const_iterator iter = extensions->begin();
        iter != extensions->end(); ++iter) {
      extension_info_[(*iter)->id()] =
          linked_ptr<ChromeURLRequestContext::ExtensionInfo>(
              new ChromeURLRequestContext::ExtensionInfo(
                  (*iter)->path(),
                  (*iter)->default_locale(),
                  (*iter)->app_extent(),
                  (*iter)->api_permissions()));
    }
  }

  if (profile->GetUserScriptMaster())
    user_script_dir_path_ = profile->GetUserScriptMaster()->user_script_dir();

  ssl_config_service_ = profile->GetSSLConfigService();

  profile_dir_path_ = profile->GetPath();
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
  context->set_cookie_policy_type(cookie_policy_type_);
  context->set_extension_info(extension_info_);
  context->set_user_script_dir_path(user_script_dir_path_);
  context->set_host_content_settings_map(host_content_settings_map_);
  context->set_host_zoom_map(host_zoom_map_);
  context->set_privacy_blacklist(privacy_blacklist_);
  context->set_transport_security_state(
      transport_security_state_);
  context->set_ssl_config_service(ssl_config_service_);
}

// ----------------------------------------------------------------------------

net::ProxyConfig* CreateProxyConfig(const CommandLine& command_line) {
  // Scan for all "enable" type proxy switches.
  static const char* proxy_switches[] = {
    switches::kProxyServer,
    switches::kProxyPacUrl,
    switches::kProxyAutoDetect,
    switches::kProxyBypassList
  };

  bool found_enable_proxy_switch = false;
  for (size_t i = 0; i < arraysize(proxy_switches); i++) {
    if (command_line.HasSwitch(proxy_switches[i])) {
      found_enable_proxy_switch = true;
      break;
    }
  }

  if (!found_enable_proxy_switch &&
      !command_line.HasSwitch(switches::kNoProxyServer)) {
    return NULL;
  }

  net::ProxyConfig* proxy_config = new net::ProxyConfig();
  if (command_line.HasSwitch(switches::kNoProxyServer)) {
    // Ignore (and warn about) all the other proxy config switches we get if
    // the --no-proxy-server command line argument is present.
    if (found_enable_proxy_switch) {
      LOG(WARNING) << "Additional command line proxy switches found when --"
                   << switches::kNoProxyServer << " was specified.";
    }
    return proxy_config;
  }

  if (command_line.HasSwitch(switches::kProxyServer)) {
    const std::wstring& proxy_server =
        command_line.GetSwitchValue(switches::kProxyServer);
    proxy_config->proxy_rules.ParseFromString(WideToASCII(proxy_server));
  }

  if (command_line.HasSwitch(switches::kProxyPacUrl)) {
    proxy_config->pac_url =
        GURL(WideToASCII(command_line.GetSwitchValue(
            switches::kProxyPacUrl)));
  }

  if (command_line.HasSwitch(switches::kProxyAutoDetect)) {
    proxy_config->auto_detect = true;
  }

  if (command_line.HasSwitch(switches::kProxyBypassList)) {
    proxy_config->ParseNoProxyList(
        WideToASCII(command_line.GetSwitchValue(
            switches::kProxyBypassList)));
  }

  return proxy_config;
}

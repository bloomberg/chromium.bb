// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_url_request_context.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/browser/net/dns_global.h"
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
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/webkit_glue.h"

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

// Create a proxy service according to the options on command line.
static net::ProxyService* CreateProxyService(URLRequestContext* context,
                                             const CommandLine& command_line) {
  scoped_ptr<net::ProxyConfig> proxy_config(CreateProxyConfig(command_line));

  bool use_v8 = !command_line.HasSwitch(switches::kWinHttpProxyResolver);
  if (use_v8 && command_line.HasSwitch(switches::kSingleProcess)) {
    // See the note about V8 multithreading in net/proxy/proxy_resolver_v8.h
    // to understand why we have this limitation.
    LOG(ERROR) << "Cannot use V8 Proxy resolver in single process mode.";
    use_v8 = false;  // Fallback to non-v8 implementation.
  }

  return net::ProxyService::Create(
      proxy_config.get(),
      use_v8,
      context,
      g_browser_process->io_thread()->message_loop(),
      g_browser_process->file_thread()->message_loop());
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOriginal(
    Profile* profile, const FilePath& cookie_store_path,
    const FilePath& disk_cache_path, int cache_size,
    ChromeAppCacheService* appcache_service) {
  DCHECK(!profile->IsOffTheRecord());
  ChromeURLRequestContext* context = new ChromeURLRequestContext(
      profile, appcache_service);

  // The appcache service uses the profile's original context for UpdateJobs.
  DCHECK(!appcache_service->request_context());
  appcache_service->set_request_context(context);

  // Global host resolver for the context.
  context->host_resolver_ = chrome_browser_net::GetGlobalHostResolver();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  context->proxy_service_ = CreateProxyService(context, command_line);

  net::HttpCache* cache =
      new net::HttpCache(context->host_resolver_,
                         context->proxy_service_,
                         context->ssl_config_service_,
                         disk_cache_path, cache_size);

  if (command_line.HasSwitch(switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  bool record_mode = chrome::kRecordModeEnabled &&
                     command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  if (record_mode || playback_mode) {
    // Don't use existing cookies and use an in-memory store.
    context->cookie_store_ = new net::CookieMonster();
    cache->set_mode(
        record_mode ? net::HttpCache::RECORD : net::HttpCache::PLAYBACK);
  }
  context->http_transaction_factory_ = cache;

  // The kWininetFtp switch is Windows specific because we have two FTP
  // implementations on Windows.
#if defined(OS_WIN)
  if (!command_line.HasSwitch(switches::kWininetFtp))
    context->ftp_transaction_factory_ =
        new net::FtpNetworkLayer(context->host_resolver_);
#else
  context->ftp_transaction_factory_ =
      new net::FtpNetworkLayer(context->host_resolver_);
#endif

  // setup cookie store
  if (!context->cookie_store_) {
    DCHECK(!cookie_store_path.empty());

    scoped_refptr<SQLitePersistentCookieStore> cookie_db =
        new SQLitePersistentCookieStore(cookie_store_path);
    context->cookie_store_ = new net::CookieMonster(cookie_db.get());
  }

  return context;
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOriginalForMedia(
    Profile* profile, const FilePath& disk_cache_path, int cache_size,
    ChromeAppCacheService* appcache_service) {
  DCHECK(!profile->IsOffTheRecord());
  return CreateRequestContextForMedia(profile, disk_cache_path, cache_size,
                                      false, appcache_service);
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOriginalForExtensions(
    Profile* profile, const FilePath& cookie_store_path) {
  DCHECK(!profile->IsOffTheRecord());
  ChromeURLRequestContext* context = new ChromeURLRequestContext(
      profile, NULL);

  // All we care about for extensions is the cookie store.
  DCHECK(!cookie_store_path.empty());

  scoped_refptr<SQLitePersistentCookieStore> cookie_db =
      new SQLitePersistentCookieStore(cookie_store_path);
  net::CookieMonster* cookie_monster = new net::CookieMonster(cookie_db.get());

  // Enable cookies for extension URLs only.
  const char* schemes[] = {chrome::kExtensionScheme};
  cookie_monster->SetCookieableSchemes(schemes, 1);
  context->cookie_store_ = cookie_monster;

  return context;
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOffTheRecord(
    Profile* profile, ChromeAppCacheService* appcache_service) {
  DCHECK(profile->IsOffTheRecord());
  ChromeURLRequestContext* context = new ChromeURLRequestContext(
      profile, appcache_service);

  // The appcache service uses the profile's original context for UpdateJobs.
  DCHECK(!appcache_service->request_context());
  appcache_service->set_request_context(context);

  // Share the same proxy service and host resolver as the original profile.
  // TODO(eroman): although ProxyService is reference counted, this sharing
  // still has a subtle dependency on the lifespan of the original profile --
  // ProxyService holds a (non referencing) pointer to the URLRequestContext
  // it uses to download PAC scripts, which in this case is the original
  // profile...
  context->host_resolver_ =
      profile->GetOriginalProfile()->GetRequestContext()->host_resolver();
  context->proxy_service_ =
      profile->GetOriginalProfile()->GetRequestContext()->proxy_service();

  net::HttpCache* cache =
      new net::HttpCache(context->host_resolver_, context->proxy_service_,
                         context->ssl_config_service_, 0);
  context->cookie_store_ = new net::CookieMonster;
  context->http_transaction_factory_ = cache;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  // The kWininetFtp switch is Windows specific because we have two FTP
  // implementations on Windows.
#if defined(OS_WIN)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kWininetFtp))
    context->ftp_transaction_factory_ =
        new net::FtpNetworkLayer(context->host_resolver_);
#else
  context->ftp_transaction_factory_ =
      new net::FtpNetworkLayer(context->host_resolver_);
#endif

  return context;
}

// static
ChromeURLRequestContext*
ChromeURLRequestContext::CreateOffTheRecordForExtensions(Profile* profile) {
  DCHECK(profile->IsOffTheRecord());
  ChromeURLRequestContext* context =
      new ChromeURLRequestContext(profile, NULL);
  net::CookieMonster* cookie_monster = new net::CookieMonster;

  // Enable cookies for extension URLs only.
  const char* schemes[] = {chrome::kExtensionScheme};
  cookie_monster->SetCookieableSchemes(schemes, 1);
  context->cookie_store_ = cookie_monster;

  return context;
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateRequestContextForMedia(
    Profile* profile, const FilePath& disk_cache_path, int cache_size,
    bool off_the_record, ChromeAppCacheService* appcache_service) {
  URLRequestContext* original_context =
      profile->GetOriginalProfile()->GetRequestContext();
  ChromeURLRequestContext* context =
      new ChromeURLRequestContext(profile, appcache_service);
  context->is_media_ = true;

  // Share the same proxy service of the common profile.
  context->proxy_service_ = original_context->proxy_service();
  // Also share the cookie store of the common profile.
  context->cookie_store_ = original_context->cookie_store();

  // Create a media cache with default size.
  // TODO(hclam): make the maximum size of media cache configurable.
  net::HttpCache* original_cache =
      original_context->http_transaction_factory()->GetCache();
  net::HttpCache* cache;
  if (original_cache) {
    // Try to reuse HttpNetworkSession in the original context, assuming that
    // HttpTransactionFactory (network_layer()) of HttpCache is implemented
    // by HttpNetworkLayer so we can reuse HttpNetworkSession within it. This
    // assumption will be invalid if the original HttpCache is constructed with
    // HttpCache(HttpTransactionFactory*, disk_cache::Backend*) constructor.
    net::HttpNetworkLayer* original_network_layer =
        static_cast<net::HttpNetworkLayer*>(original_cache->network_layer());
    cache = new net::HttpCache(original_network_layer->GetSession(),
                               disk_cache_path, cache_size);
  } else {
    // If original HttpCache doesn't exist, simply construct one with a whole
    // new set of network stack.
    cache = new net::HttpCache(original_context->host_resolver(),
                               original_context->proxy_service(),
                               original_context->ssl_config_service(),
                               disk_cache_path, cache_size);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableByteRangeSupport))
    cache->set_enable_range_support(false);

  cache->set_type(net::MEDIA_CACHE);
  context->http_transaction_factory_ = cache;
  return context;
}

ChromeURLRequestContext::ChromeURLRequestContext(
    Profile* profile, ChromeAppCacheService* appcache_service)
    : appcache_service_(appcache_service),
      prefs_(profile->GetPrefs()),
      is_media_(false),
      is_off_the_record_(profile->IsOffTheRecord()) {
  // Set up Accept-Language and Accept-Charset header values
  accept_language_ = net::HttpUtil::GenerateAcceptLanguageHeader(
      WideToASCII(prefs_->GetString(prefs::kAcceptLanguages)));
  std::string default_charset =
      WideToASCII(prefs_->GetString(prefs::kDefaultCharset));
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

  cookie_policy_.set_type(net::CookiePolicy::FromInt(
      prefs_->GetInteger(prefs::kCookieBehavior)));

  blacklist_ = profile->GetBlacklist();

  strict_transport_security_state_ = profile->GetStrictTransportSecurityState();

  if (profile->GetExtensionsService()) {
    const ExtensionList* extensions =
        profile->GetExtensionsService()->extensions();
    for (ExtensionList::const_iterator iter = extensions->begin();
        iter != extensions->end(); ++iter) {
      extension_paths_[(*iter)->id()] = (*iter)->path();
    }
  }

  if (profile->GetUserScriptMaster())
    user_script_dir_path_ = profile->GetUserScriptMaster()->user_script_dir();

  prefs_->AddPrefObserver(prefs::kAcceptLanguages, this);
  prefs_->AddPrefObserver(prefs::kCookieBehavior, this);
  prefs_->AddPrefObserver(prefs::kDefaultCharset, this);

  ssl_config_service_ = profile->GetSSLConfigService();
}

ChromeURLRequestContext::ChromeURLRequestContext(
    ChromeURLRequestContext* other) {
  // Set URLRequestContext members
  host_resolver_ = other->host_resolver_;
  proxy_service_ = other->proxy_service_;
  ssl_config_service_ = other->ssl_config_service_;
  http_transaction_factory_ = other->http_transaction_factory_;
  ftp_transaction_factory_ = other->ftp_transaction_factory_;
  cookie_store_ = other->cookie_store_;
  cookie_policy_.set_type(other->cookie_policy_.type());
  strict_transport_security_state_ = other->strict_transport_security_state_;
  accept_language_ = other->accept_language_;
  accept_charset_ = other->accept_charset_;
  referrer_charset_ = other->referrer_charset_;

  // Set ChromeURLRequestContext members
  appcache_service_ = other->appcache_service_;
  extension_paths_ = other->extension_paths_;
  user_script_dir_path_ = other->user_script_dir_path_;
  prefs_ = other->prefs_;
  blacklist_ = other->blacklist_;
  is_media_ = other->is_media_;
  is_off_the_record_ = other->is_off_the_record_;
}

// NotificationObserver implementation.
void ChromeURLRequestContext::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (NotificationType::PREF_CHANGED == type) {
    std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
    PrefService* prefs = Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kAcceptLanguages) {
      std::string accept_language =
          WideToASCII(prefs->GetString(prefs::kAcceptLanguages));
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &ChromeURLRequestContext::OnAcceptLanguageChange,
                            accept_language));
    } else if (*pref_name_in == prefs::kCookieBehavior) {
      net::CookiePolicy::Type policy_type = net::CookiePolicy::FromInt(
          prefs_->GetInteger(prefs::kCookieBehavior));
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &ChromeURLRequestContext::OnCookiePolicyChange,
                            policy_type));
    } else if (*pref_name_in == prefs::kDefaultCharset) {
      std::string default_charset =
          WideToASCII(prefs->GetString(prefs::kDefaultCharset));
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &ChromeURLRequestContext::OnDefaultCharsetChange,
                            default_charset));
    }
  } else {
    NOTREACHED();
  }
}

void ChromeURLRequestContext::CleanupOnUIThread() {
  // Unregister for pref notifications.
  prefs_->RemovePrefObserver(prefs::kAcceptLanguages, this);
  prefs_->RemovePrefObserver(prefs::kCookieBehavior, this);
  prefs_->RemovePrefObserver(prefs::kDefaultCharset, this);
  prefs_ = NULL;

  registrar_.RemoveAll();
}

FilePath ChromeURLRequestContext::GetPathForExtension(const std::string& id) {
  ExtensionPaths::iterator iter = extension_paths_.find(id);
  if (iter != extension_paths_.end()) {
    return iter->second;
  } else {
    return FilePath();
  }
}

const std::string& ChromeURLRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}

bool ChromeURLRequestContext::InterceptCookie(const URLRequest* request,
                                              std::string* cookie) {
  const URLRequest::UserData* d =
      request->GetUserData(&Blacklist::kRequestDataKey);
  if (d) {
    const Blacklist::Match* match = static_cast<const Blacklist::Match*>(d);
    if (match->attributes() & Blacklist::kDontStoreCookies) {
      NotificationService::current()->Notify(
          NotificationType::BLACKLIST_BLOCKED_RESOURCE,
          Source<const ChromeURLRequestContext>(this),
          Details<const URLRequest>(request));

      cookie->clear();
      return false;
    }
    if (match->attributes() & Blacklist::kDontPersistCookies) {
      *cookie = Blacklist::StripCookieExpiry(*cookie);
    }
  }
  return true;
}

bool ChromeURLRequestContext::AllowSendingCookies(const URLRequest* request)
    const {
  const URLRequest::UserData* d =
      request->GetUserData(&Blacklist::kRequestDataKey);
  if (d) {
    const Blacklist::Match* match = static_cast<const Blacklist::Match*>(d);
    if (match->attributes() & Blacklist::kDontSendCookies) {
      NotificationService::current()->Notify(
          NotificationType::BLACKLIST_BLOCKED_RESOURCE,
          Source<const ChromeURLRequestContext>(this),
          Details<const URLRequest>(request));

      return false;
    }
  }
  return true;
}

void ChromeURLRequestContext::OnAcceptLanguageChange(
    const std::string& accept_language) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  accept_language_ =
      net::HttpUtil::GenerateAcceptLanguageHeader(accept_language);
}

void ChromeURLRequestContext::OnCookiePolicyChange(
    net::CookiePolicy::Type type) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  cookie_policy_.set_type(type);
}

void ChromeURLRequestContext::OnDefaultCharsetChange(
    const std::string& default_charset) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  referrer_charset_ = default_charset;
  accept_charset_ =
      net::HttpUtil::GenerateAcceptCharsetHeader(default_charset);
}

void ChromeURLRequestContext::OnNewExtensions(const std::string& id,
                                              const FilePath& path) {
  if (!is_off_the_record_)
    extension_paths_[id] = path;
}

void ChromeURLRequestContext::OnUnloadedExtension(const std::string& id) {
  if (is_off_the_record_)
    return;
  ExtensionPaths::iterator iter = extension_paths_.find(id);
  DCHECK(iter != extension_paths_.end());
  extension_paths_.erase(iter);
}

ChromeURLRequestContext::~ChromeURLRequestContext() {
  DCHECK(NULL == prefs_);

  if (appcache_service_.get() && appcache_service_->request_context() == this)
    appcache_service_->set_request_context(NULL);

  NotificationService::current()->Notify(
      NotificationType::URL_REQUEST_CONTEXT_RELEASED,
      Source<URLRequestContext>(this),
      NotificationService::NoDetails());

  delete ftp_transaction_factory_;
  delete http_transaction_factory_;
}

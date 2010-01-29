// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
#define CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_

#include "base/file_path.h"
#include "base/linked_ptr.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/common/appcache/chrome_appcache_service.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_service.h"
#include "net/url_request/url_request_context.h"

class Blacklist;
class CommandLine;
class Profile;

namespace net {
class ProxyConfig;
}

class ChromeURLRequestContext;
class ChromeURLRequestContextFactory;
class IOThread;

// Subclass of URLRequestContext which can be used to store extra information
// for requests.
//
// All methods of this class must be called from the IO thread,
// including the constructor and destructor.
class ChromeURLRequestContext : public URLRequestContext {
 public:
  // Maintains some extension-related state we need on the IO thread.
  // TODO(aa): It would be cool if the Extension objects in ExtensionsService
  // could be immutable and ref-counted so that we could use them directly from
  // both threads. There is only a small amount of mutable state in Extension.
  struct ExtensionInfo {
    ExtensionInfo(const FilePath& path, const std::string& default_locale,
                  const std::vector<GURL>& web_origins,
                  const std::vector<std::string>& api_permissions)
        : path(path), default_locale(default_locale),
          web_origins(web_origins), api_permissions(api_permissions) {
    }
    FilePath path;
    std::string default_locale;
    std::vector<GURL> web_origins;
    std::vector<std::string> api_permissions;
  };

  // Map of extension info by extension id.
  typedef std::map<std::string, linked_ptr<ExtensionInfo> > ExtensionInfoMap;

  ChromeURLRequestContext();

  // Gets the path to the directory for the specified extension.
  FilePath GetPathForExtension(const std::string& id);

  // Returns an empty string if the extension with |id| doesn't have a default
  // locale.
  std::string GetDefaultLocaleForExtension(const std::string& id);

  // Determine whether a URL has access to the specified extension permission.
  // TODO(aa): This will eventually have to take an additional parameter: the
  // ID of a specific extension to check, since |url| could show up in multiple
  // extensions.
  bool CheckURLAccessToExtensionPermission(const GURL& url,
                                           const std::string& application_id,
                                           const char* permission_name);

  // Gets the path to the directory user scripts are stored in.
  FilePath user_script_dir_path() const {
    return user_script_dir_path_;
  }

  // Gets the appcache service to be used for requests in this context.
  // May be NULL if requests for this context aren't subject to appcaching.
  ChromeAppCacheService* appcache_service() const {
    return appcache_service_.get();
  }

  bool is_off_the_record() const {
    return is_off_the_record_;
  }
  bool is_media() const {
    return is_media_;
  }

  virtual const std::string& GetUserAgent(const GURL& url) const;

  // Returns true if cookies can be added to request.
  virtual bool InterceptRequestCookies(const URLRequest* request,
                                       const std::string& cookie) const;

  // Returns true if response cookies should be stored.
  virtual bool InterceptResponseCookie(const URLRequest* request,
                                       const std::string& cookie) const;

  const HostContentSettingsMap* host_content_settings_map() const {
    return host_content_settings_map_;
  }

  const HostZoomMap* host_zoom_map() const { return host_zoom_map_; }

  // Gets the Privacy Blacklist, if any for this context.
  const Blacklist* GetPrivacyBlacklist() const;

  // Callback for when new extensions are loaded. Takes ownership of
  // |extension_info|.
  void OnNewExtensions(
      const std::string& id,
      ChromeURLRequestContext::ExtensionInfo* extension_info);

  // Callback for when an extension is unloaded.
  void OnUnloadedExtension(const std::string& id);

 protected:
  // Copies the dependencies from |other| into |this|. If you use this
  // constructor, then you should hold a reference to |other|, as we
  // depend on |other| being alive.
  explicit ChromeURLRequestContext(ChromeURLRequestContext* other);
  virtual ~ChromeURLRequestContext();

 public:
  // Setters to simplify initializing from factory objects.

  void set_accept_language(const std::string& accept_language) {
    accept_language_ = accept_language;
  }
  void set_accept_charset(const std::string& accept_charset) {
    accept_charset_ = accept_charset;
  }
  void set_referrer_charset(const std::string& referrer_charset) {
    referrer_charset_ = referrer_charset;
  }
  void set_cookie_policy_type(net::CookiePolicy::Type type) {
    cookie_policy_.set_type(type);
  }
  void set_extension_info(
      const ChromeURLRequestContext::ExtensionInfoMap& info) {
    extension_info_ = info;
  }
  void set_transport_security_state(
      net::TransportSecurityState* state) {
    transport_security_state_ = state;
  }
  void set_ssl_config_service(net::SSLConfigService* service) {
    ssl_config_service_ = service;
  }
  void set_host_resolver(net::HostResolver* resolver) {
    host_resolver_ = resolver;
  }
  void set_http_transaction_factory(net::HttpTransactionFactory* factory) {
    http_transaction_factory_ = factory;
  }
  void set_ftp_transaction_factory(net::FtpTransactionFactory* factory) {
    ftp_transaction_factory_ = factory;
  }
  void set_cookie_store(net::CookieStore* cookie_store) {
    cookie_store_ = cookie_store;
  }
  void set_proxy_service(net::ProxyService* service) {
    proxy_service_ = service;
  }
  void set_user_script_dir_path(const FilePath& path) {
    user_script_dir_path_ = path;
  }
  void set_is_off_the_record(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }
  void set_is_media(bool is_media) {
    is_media_ = is_media;
  }
  void set_host_content_settings_map(
      HostContentSettingsMap* host_content_settings_map) {
    host_content_settings_map_ = host_content_settings_map;
  }
  void set_host_zoom_map(HostZoomMap* host_zoom_map) {
    host_zoom_map_ = host_zoom_map;
  }
  void set_privacy_blacklist(const Blacklist* privacy_blacklist);
  void set_appcache_service(ChromeAppCacheService* service) {
    appcache_service_ = service;
  }

  // Callback for when the accept language changes.
  void OnAcceptLanguageChange(const std::string& accept_language);

  // Callback for when the cookie policy changes.
  void OnCookiePolicyChange(net::CookiePolicy::Type type);

  // Callback for when the default charset changes.
  void OnDefaultCharsetChange(const std::string& default_charset);

 protected:
  ExtensionInfoMap extension_info_;

  // Path to the directory user scripts are stored in.
  FilePath user_script_dir_path_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;
  HostContentSettingsMap* host_content_settings_map_;
  scoped_refptr<HostZoomMap> host_zoom_map_;

  const Blacklist* privacy_blacklist_;

  bool is_media_;
  bool is_off_the_record_;

 private:
  // Blacklist implementation of InterceptRequestCookie and
  // InterceptResponseCookie. Returns true if cookies are allowed and false
  // if the request matches a Blacklist rule and cookies should be blocked.
  bool InterceptCookie(const URLRequest* request,
                       const std::string& cookie) const;

  // Filter for url_request_tracker(), that prevents "chrome://" requests from
  // being tracked by "about:net-internals".
  static bool ShouldTrackRequest(const GURL& url);

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContext);
};

// A URLRequestContextGetter subclass used by the browser. This returns a
// subclass of URLRequestContext which can be used to store extra information
// about requests.
//
// Most methods are expected to be called on the UI thread, except for
// the destructor and GetURLRequestContext().
class ChromeURLRequestContextGetter : public URLRequestContextGetter,
                                      public NotificationObserver {
 public:
  // Constructs a ChromeURLRequestContextGetter that will use |factory| to
  // create the ChromeURLRequestContext. If |profile| is non-NULL, then the
  // ChromeURLRequestContextGetter will additionally watch the preferences for
  // changes to charset/language and CleanupOnUIThread() will need to be
  // called to unregister.
  ChromeURLRequestContextGetter(Profile* profile,
                                ChromeURLRequestContextFactory* factory);

  // Note that GetURLRequestContext() can only be called from the IO
  // thread (it will assert otherwise). GetCookieStore() however can
  // be called from any thread.
  //
  // URLRequestContextGetter implementation.
  virtual URLRequestContext* GetURLRequestContext();
  virtual net::CookieStore* GetCookieStore();

  // Convenience overload of GetURLRequestContext() that returns a
  // ChromeURLRequestContext* rather than a URLRequestContext*.
  ChromeURLRequestContext* GetIOContext() {
    return reinterpret_cast<ChromeURLRequestContext*>(GetURLRequestContext());
  }

  // Create an instance for use with an 'original' (non-OTR) profile. This is
  // expected to get called on the UI thread.
  static ChromeURLRequestContextGetter* CreateOriginal(
      Profile* profile, const FilePath& cookie_store_path,
      const FilePath& disk_cache_path, int cache_size);

  // Create an instance for an original profile for media. This is expected to
  // get called on UI thread. This method takes a profile and reuses the
  // 'original' URLRequestContext for common files.
  static ChromeURLRequestContextGetter* CreateOriginalForMedia(
      Profile* profile, const FilePath& disk_cache_path, int cache_size);

  // Create an instance for an original profile for extensions. This is expected
  // to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOriginalForExtensions(
      Profile* profile, const FilePath& cookie_store_path);

  // Create an instance for use with an OTR profile. This is expected to get
  // called on the UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecord(Profile* profile);

  // Create an instance of request context for OTR profile for extensions.
  static ChromeURLRequestContextGetter* CreateOffTheRecordForExtensions(
      Profile* profile);

  // Clean up UI thread resources. This is expected to get called on the UI
  // thread before the instance is deleted on the IO thread.
  void CleanupOnUIThread();

  // These methods simply forward to the corresponding methods on
  // ChromeURLRequestContext. Takes ownership of |extension_info|.
  void OnNewExtensions(
      const std::string& extension_id,
      ChromeURLRequestContext::ExtensionInfo* extension_info);
  void OnUnloadedExtension(const std::string& id);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Must be called on the IO thread.
  virtual ~ChromeURLRequestContextGetter();

  // Registers an observer on |profile|'s preferences which will be used
  // to update the context when the default language and charset change.
  void RegisterPrefsObserver(Profile* profile);

  // Creates a request context for media resources from a regular request
  // context. This helper method is called from CreateOriginalForMedia and
  // CreateOffTheRecordForMedia.
  static ChromeURLRequestContextGetter* CreateRequestContextForMedia(
      Profile* profile, const FilePath& disk_cache_path, int cache_size,
      bool off_the_record);

  // These methods simply forward to the corresponding method on
  // ChromeURLRequestContext.
  void OnAcceptLanguageChange(const std::string& accept_language);
  void OnCookiePolicyChange(net::CookiePolicy::Type type);
  void OnDefaultCharsetChange(const std::string& default_charset);

  // Saves the cookie store to |result| and signals |completion|.
  void GetCookieStoreAsyncHelper(base::WaitableEvent* completion,
                                 net::CookieStore** result);

  // Access only from the UI thread.
  PrefService* prefs_;

  // Deferred logic for creating a ChromeURLRequestContext.
  // Access only from the IO thread.
  scoped_ptr<ChromeURLRequestContextFactory> factory_;

  // NULL if not yet initialized. Otherwise, it is the URLRequestContext
  // instance that was lazilly created by GetURLRequestContext.
  // Access only from the IO thread.
  scoped_refptr<URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContextGetter);
};

// Base class for a ChromeURLRequestContext factory. This includes
// the shared functionality like extracting the default language/charset
// from a profile.
//
// Except for the constructor, all methods of this class must be called from
// the IO thread.
class ChromeURLRequestContextFactory {
 public:
  // Extract properties of interested from |profile|, for setting later into
  // a ChromeURLRequestContext using ApplyProfileParametersToContext().
  explicit ChromeURLRequestContextFactory(Profile* profile);

  virtual ~ChromeURLRequestContextFactory();

  // Called to create a new instance (will only be called once).
  virtual ChromeURLRequestContext* Create() = 0;

 protected:
  IOThread* io_thread() { return io_thread_; }

  // Assigns this factory's properties to |context|.
  void ApplyProfileParametersToContext(ChromeURLRequestContext* context);

  // Values extracted from the Profile.
  //
  // NOTE: If you add any parameters here, keep it in sync with
  // ApplyProfileParametersToContext().
  bool is_media_;
  bool is_off_the_record_;
  std::string accept_language_;
  std::string accept_charset_;
  std::string referrer_charset_;
  net::CookiePolicy::Type cookie_policy_type_;
  ChromeURLRequestContext::ExtensionInfoMap extension_info_;
  // TODO(aa): I think this can go away now as we no longer support standalone
  // user scripts.
  FilePath user_script_dir_path_;
  HostContentSettingsMap* host_content_settings_map_;
  scoped_refptr<HostZoomMap> host_zoom_map_;
  const Blacklist* privacy_blacklist_;
  net::TransportSecurityState* transport_security_state_;
  scoped_refptr<net::SSLConfigService> ssl_config_service_;

  FilePath profile_dir_path_;

 private:
  IOThread* const io_thread_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContextFactory);
};

// Creates a proxy configuration using the overrides specified on the command
// line. Returns NULL if the system defaults should be used instead.
net::ProxyConfig* CreateProxyConfig(const CommandLine& command_line);

#endif  // CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_

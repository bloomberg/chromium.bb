// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
#define CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_

#include "base/file_path.h"
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

// TODO(eroman): Cleanup the declaration order in this file -- it is all
//               wonky to try and minimize awkward deltas.

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

  // Must be called on the IO thread.
  virtual ~ChromeURLRequestContextGetter();

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

  // These methods simply forward to the corresponding method on
  // ChromeURLRequestContext.
  void OnNewExtensions(const std::string& id, const FilePath& path);
  void OnUnloadedExtension(const std::string& id);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
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

// Subclass of URLRequestContext which can be used to store extra information
// for requests.
//
// All methods of this class must be called from the IO thread,
// including the constructor and destructor.
class ChromeURLRequestContext : public URLRequestContext {
 public:
  typedef std::map<std::string, FilePath> ExtensionPaths;

  ChromeURLRequestContext();
  virtual ~ChromeURLRequestContext();

  // Gets the path to the directory for the specified extension.
  FilePath GetPathForExtension(const std::string& id);

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
  const ExtensionPaths& extension_paths() const {
    return extension_paths_;
  }

  virtual const std::string& GetUserAgent(const GURL& url) const;

  virtual bool InterceptCookie(const URLRequest* request, std::string* cookie);

  virtual bool AllowSendingCookies(const URLRequest* request) const;

  // Gets the Privacy Blacklist, if any for this context.
  const Blacklist* blacklist() const { return blacklist_; }

  // Callback for when new extensions are loaded.
  void OnNewExtensions(const std::string& id, const FilePath& path);

  // Callback for when an extension is unloaded.
  void OnUnloadedExtension(const std::string& id);

 protected:
  // Copies the dependencies from |other| into |this|. If you use this
  // constructor, then you should hold a reference to |other|, as we
  // depend on |other| being alive.
  ChromeURLRequestContext(ChromeURLRequestContext* other);

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
  void set_strict_transport_security_state(
      net::StrictTransportSecurityState* state) {
    strict_transport_security_state_ = state;
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
  void set_extension_paths(const ExtensionPaths& paths) {
    extension_paths_ = paths;
  }
  void set_blacklist(const Blacklist* blacklist) {
    blacklist_ = blacklist;
  }
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
  // Maps extension IDs to paths on disk. This is initialized in the
  // construtor and updated when extensions changed.
  ExtensionPaths extension_paths_;

  // Path to the directory user scripts are stored in.
  FilePath user_script_dir_path_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;

  const Blacklist* blacklist_;
  bool is_media_;
  bool is_off_the_record_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContext);
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
  ChromeURLRequestContextFactory(Profile* profile);

  virtual ~ChromeURLRequestContextFactory();

  // Called to create a new instance (will only be called once).
  virtual ChromeURLRequestContext* Create() = 0;

 protected:
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
  ChromeURLRequestContext::ExtensionPaths extension_paths_;
  FilePath user_script_dir_path_;
  Blacklist* blacklist_;
  net::StrictTransportSecurityState* strict_transport_security_state_;
  scoped_refptr<net::SSLConfigService> ssl_config_service_;

  FilePath profile_dir_path_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContextFactory);
};

// Creates a proxy configuration using the overrides specified on the command
// line. Returns NULL if the system defaults should be used instead.
net::ProxyConfig* CreateProxyConfig(const CommandLine& command_line);

#endif  // CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_

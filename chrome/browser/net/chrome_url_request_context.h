// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
#define CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "chrome/browser/appcache/chrome_appcache_service.h"
#include "chrome/browser/chrome_blob_storage_context.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_cookie_policy.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/url_request/url_request_context.h"
#include "webkit/database/database_tracker.h"
#include "webkit/fileapi/sandboxed_file_system_context.h"

class CommandLine;
class PrefService;
class Profile;

namespace net {
class DnsCertProvenanceChecker;
class NetworkDelegate;
}

class ChromeURLRequestContext;
class ChromeURLRequestContextFactory;

// Subclass of net::URLRequestContext which can be used to store extra
// information for requests.
//
// All methods of this class must be called from the IO thread,
// including the constructor and destructor.
class ChromeURLRequestContext : public net::URLRequestContext {
 public:
  ChromeURLRequestContext();

  // Gets the path to the directory user scripts are stored in.
  FilePath user_script_dir_path() const {
    return user_script_dir_path_;
  }

  // Gets the appcache service to be used for requests in this context.
  // May be NULL if requests for this context aren't subject to appcaching.
  ChromeAppCacheService* appcache_service() const {
    return appcache_service_.get();
  }

  // Gets the database tracker associated with this context's profile.
  webkit_database::DatabaseTracker* database_tracker() const {
    return database_tracker_.get();
  }

  // Gets the blob storage context associated with this context's profile.
  ChromeBlobStorageContext* blob_storage_context() const {
    return blob_storage_context_.get();
  }

  // Gets the file system host context with this context's profile.
  fileapi::SandboxedFileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  bool is_off_the_record() const {
    return is_off_the_record_;
  }

  virtual const std::string& GetUserAgent(const GURL& url) const;

  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_;
  }

  const HostZoomMap* host_zoom_map() const { return host_zoom_map_; }

  const ExtensionInfoMap* extension_info_map() const {
    return extension_info_map_;
  }

  PrerenderManager* prerender_manager() {
    return prerender_manager_.get();
  }

 protected:
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
  void set_transport_security_state(
      net::TransportSecurityState* state) {
    transport_security_state_ = state;
  }
  void set_ssl_config_service(net::SSLConfigService* service) {
    ssl_config_service_ = service;
  }
  void set_dns_cert_checker(net::DnsCertProvenanceChecker* ctx) {
    dns_cert_checker_.reset(ctx);
  }
  void set_ftp_transaction_factory(net::FtpTransactionFactory* factory) {
    ftp_transaction_factory_ = factory;
  }
  void set_cookie_policy(ChromeCookiePolicy* cookie_policy) {
    chrome_cookie_policy_ = cookie_policy;  // Take a strong reference.
    cookie_policy_ = cookie_policy;
  }
  void set_user_script_dir_path(const FilePath& path) {
    user_script_dir_path_ = path;
  }
  void set_is_off_the_record(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }
  void set_host_content_settings_map(
      HostContentSettingsMap* host_content_settings_map) {
    host_content_settings_map_ = host_content_settings_map;
  }
  void set_host_zoom_map(HostZoomMap* host_zoom_map) {
    host_zoom_map_ = host_zoom_map;
  }
  void set_appcache_service(ChromeAppCacheService* service) {
    appcache_service_ = service;
  }
  void set_database_tracker(webkit_database::DatabaseTracker* tracker) {
    database_tracker_ = tracker;
  }
  void set_blob_storage_context(ChromeBlobStorageContext* context) {
    blob_storage_context_ = context;
  }
  void set_file_system_context(fileapi::SandboxedFileSystemContext* context) {
    file_system_context_ = context;
  }
  void set_extension_info_map(ExtensionInfoMap* map) {
    extension_info_map_ = map;
  }
  void set_network_delegate(
      net::HttpNetworkDelegate* network_delegate) {
    network_delegate_ = network_delegate;
  }
  void set_prerender_manager(PrerenderManager* prerender_manager) {
    prerender_manager_ = prerender_manager;
  }

  // Callback for when the accept language changes.
  void OnAcceptLanguageChange(const std::string& accept_language);

  // Callback for when the default charset changes.
  void OnDefaultCharsetChange(const std::string& default_charset);

 protected:
  // Path to the directory user scripts are stored in.
  FilePath user_script_dir_path_;

  // TODO(willchan): Make these non-refcounted.
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;
  scoped_refptr<ChromeCookiePolicy> chrome_cookie_policy_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<HostZoomMap> host_zoom_map_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  scoped_refptr<fileapi::SandboxedFileSystemContext> file_system_context_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  scoped_refptr<PrerenderManager> prerender_manager_;

  bool is_off_the_record_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContext);
};

// A URLRequestContextGetter subclass used by the browser. This returns a
// subclass of net::URLRequestContext which can be used to store extra
// information about requests.
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
  // thread (it will assert otherwise). GetCookieStore() and
  // GetIOMessageLoopProxy however can be called from any thread.
  //
  // URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext();
  virtual net::CookieStore* GetCookieStore();
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const;

  // Releases |url_request_context_|.  It's invalid to call
  // GetURLRequestContext() after this point.
  void ReleaseURLRequestContext();

  // Convenience overload of GetURLRequestContext() that returns a
  // ChromeURLRequestContext* rather than a net::URLRequestContext*.
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
  // 'original' net::URLRequestContext for common files.
  static ChromeURLRequestContextGetter* CreateOriginalForMedia(
      Profile* profile, const FilePath& disk_cache_path, int cache_size);

  // Create an instance for an original profile for extensions. This is expected
  // to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOriginalForExtensions(
      Profile* profile, const FilePath& cookie_store_path);

  // Create an instance for use with an OTR profile. This is expected to get
  // called on the UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecord(Profile* profile);

  // Create an instance for an OTR profile for extensions. This is expected
  // to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecordForExtensions(
      Profile* profile);

  // Clean up UI thread resources. This is expected to get called on the UI
  // thread before the instance is deleted on the IO thread.
  void CleanupOnUIThread();

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
  void OnDefaultCharsetChange(const std::string& default_charset);
  void OnClearSiteDataOnExitChange(bool clear_site_data);

  // Saves the cookie store to |result| and signals |completion|.
  void GetCookieStoreAsyncHelper(base::WaitableEvent* completion,
                                 net::CookieStore** result);

  PrefChangeRegistrar registrar_;

  // |io_thread_| is always valid during the lifetime of |this| since |this| is
  // deleted on the IO thread.
  IOThread* const io_thread_;

  // Deferred logic for creating a ChromeURLRequestContext.
  // Access only from the IO thread.
  scoped_ptr<ChromeURLRequestContextFactory> factory_;

  // NULL if not yet initialized. Otherwise, it is the net::URLRequestContext
  // instance that was lazilly created by GetURLRequestContext.
  // Access only from the IO thread.
  scoped_refptr<net::URLRequestContext> url_request_context_;

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
  bool clear_local_state_on_exit_;
  std::string accept_language_;
  std::string accept_charset_;
  std::string referrer_charset_;

  // TODO(aa): I think this can go away now as we no longer support standalone
  // user scripts.
  // TODO(willchan): Make these non-refcounted.
  FilePath user_script_dir_path_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;
  scoped_refptr<HostZoomMap> host_zoom_map_;
  scoped_refptr<net::TransportSecurityState> transport_security_state_;
  scoped_refptr<net::SSLConfigService> ssl_config_service_;
  scoped_refptr<net::CookieMonster::Delegate> cookie_monster_delegate_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  scoped_refptr<fileapi::SandboxedFileSystemContext> file_system_context_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  scoped_refptr<PrerenderManager> prerender_manager_;

  FilePath profile_dir_path_;

 private:
  IOThread* const io_thread_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContextFactory);
};

#endif  // CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_

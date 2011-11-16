// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/browser/resource_context.h"
#include "net/base/cookie_monster.h"

class ChromeAppCacheService;
class ChromeBlobStorageContext;
class CookieSettings;
class DesktopNotificationService;
class DownloadIdFactory;
class ExtensionInfoMap;
class HostContentSettingsMap;
class HostZoomMap;
class IOThread;
class Profile;
class ProtocolHandlerRegistry;
class TransportSecurityPersister;

namespace fileapi {
class FileSystemContext;
}  // namespace fileapi

namespace media_stream {
class MediaStreamManager;
}  // namespace media_stream

namespace net {
class CookieStore;
class DnsCertProvenanceChecker;
class FraudulentCertificateReporter;
class HttpTransactionFactory;
class OriginBoundCertService;
class ProxyConfigService;
class ProxyService;
class SSLConfigService;
class TransportSecurityState;
}  // namespace net

namespace policy {
class URLBlacklistManager;
}  // namespace policy

namespace quota {
class QuotaManager;
};  // namespace quota

namespace webkit_database {
class DatabaseTracker;
}  // webkit_database

// Conceptually speaking, the ProfileIOData represents data that lives on the IO
// thread that is owned by a Profile, such as, but not limited to, network
// objects like CookieMonster, HttpTransactionFactory, etc.  Profile owns
// ProfileIOData, but will make sure to delete it on the IO thread (except
// possibly in unit tests where there is no IO thread).
class ProfileIOData {
 public:
  virtual ~ProfileIOData();

  // Returns true if |scheme| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if |url| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledURL(const GURL& url);

  // Called by Profile.
  const content::ResourceContext& GetResourceContext() const;
  ChromeURLDataManagerBackend* GetChromeURLDataManagerBackend() const;

  // These should only be called at most once each. Ownership is reversed when
  // they get called, from ProfileIOData owning ChromeURLRequestContext to vice
  // versa.
  scoped_refptr<ChromeURLRequestContext> GetMainRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetMediaRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetExtensionsRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetIsolatedAppRequestContext(
      scoped_refptr<ChromeURLRequestContext> main_context,
      const std::string& app_id) const;

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that profile.
  ExtensionInfoMap* GetExtensionInfoMap() const;
  HostContentSettingsMap* GetHostContentSettingsMap() const;
  CookieSettings* GetCookieSettings() const;
  DesktopNotificationService* GetNotificationService() const;

  BooleanPrefMember* clear_local_state_on_exit()  const {
    return &clear_local_state_on_exit_;
  }

  ChromeURLRequestContext* extensions_request_context() const {
    return extensions_request_context_.get();
  }

  BooleanPrefMember* safe_browsing_enabled() const {
    return &safe_browsing_enabled_;
  }

  net::TransportSecurityState* transport_security_state() const {
    return transport_security_state_.get();
  }

 protected:
  class AppRequestContext : public ChromeURLRequestContext {
   public:
    AppRequestContext();
    virtual ~AppRequestContext();

    void SetCookieStore(net::CookieStore* cookie_store);
    void SetHttpTransactionFactory(net::HttpTransactionFactory* http_factory);

   private:
    scoped_refptr<net::CookieStore> cookie_store_;
    scoped_ptr<net::HttpTransactionFactory> http_factory_;
  };

  // Created on the UI thread, read on the IO thread during ProfileIOData lazy
  // initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    FilePath path;
    bool is_incognito;
    bool clear_local_state_on_exit;
    std::string accept_language;
    std::string accept_charset;
    std::string referrer_charset;
    IOThread* io_thread;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;
    scoped_refptr<CookieSettings> cookie_settings;
    scoped_refptr<HostZoomMap> host_zoom_map;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_refptr<net::CookieMonster::Delegate> cookie_monster_delegate;
    scoped_refptr<webkit_database::DatabaseTracker> database_tracker;
    scoped_refptr<ChromeAppCacheService> appcache_service;
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context;
    scoped_refptr<fileapi::FileSystemContext> file_system_context;
    scoped_refptr<quota::QuotaManager> quota_manager;
    scoped_refptr<ExtensionInfoMap> extension_info_map;
    DesktopNotificationService* notification_service;
    scoped_refptr<ProtocolHandlerRegistry> protocol_handler_registry;
    // We need to initialize the ProxyConfigService from the UI thread
    // because on linux it relies on initializing things through gconf,
    // and needs to be on the main thread.
    scoped_ptr<net::ProxyConfigService> proxy_config_service;
    // The profile this struct was populated from. It's passed as a void* to
    // ensure it's not accidently used on the IO thread. Before using it on the
    // UI thread, call ProfileManager::IsValidProfile to ensure it's alive.
    void* profile;
  };

  explicit ProfileIOData(bool is_incognito);

  void InitializeOnUIThread(Profile* profile);
  void ApplyProfileParamsToContext(ChromeURLRequestContext* context) const;

  // Lazy initializes the ProfileIOData object the first time a request context
  // is requested. The lazy logic is implemented here. The actual initialization
  // is done in LazyInitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  void LazyInitialize() const;

  // Called when the profile is destroyed.
  void ShutdownOnUIThread();

  BooleanPrefMember* enable_referrers() const {
    return &enable_referrers_;
  }

  ChromeURLDataManagerBackend* chrome_url_data_manager_backend() const {
    return chrome_url_data_manager_backend_.get();
  }

  // An OriginBoundCertService object is created by a derived class of
  // ProfileIOData, and the derived class calls this method to set the
  // origin_bound_cert_service_ member and transfers ownership to the base
  // class.
  void set_origin_bound_cert_service(
      net::OriginBoundCertService* origin_bound_cert_service) const;

  net::NetworkDelegate* network_delegate() const {
    return network_delegate_.get();
  }

  net::DnsCertProvenanceChecker* dns_cert_checker() const {
    return dns_cert_checker_.get();
  }

  net::FraudulentCertificateReporter* fraudulent_certificate_reporter() const {
    return fraudulent_certificate_reporter_.get();
  }

  net::ProxyService* proxy_service() const {
    return proxy_service_.get();
  }

  net::URLRequestJobFactory* job_factory() const {
    return job_factory_.get();
  }

  ChromeURLRequestContext* main_request_context() const {
    return main_request_context_;
  }

 private:
  class ResourceContext : public content::ResourceContext {
   public:
    explicit ResourceContext(const ProfileIOData* io_data);
    virtual ~ResourceContext();

   private:
    virtual void EnsureInitialized() const;

    const ProfileIOData* const io_data_;
  };

  typedef base::hash_map<std::string, scoped_refptr<ChromeURLRequestContext> >
      AppRequestContextMap;

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does the actual initialization of the ProfileIOData subtype. Subtypes
  // should use the static helper functions above to implement this.
  virtual void LazyInitializeInternal(ProfileParams* profile_params) const = 0;

  // Does an on-demand initialization of a RequestContext for the given
  // isolated app.
  virtual scoped_refptr<ChromeURLRequestContext> InitializeAppRequestContext(
      scoped_refptr<ChromeURLRequestContext> main_context,
      const std::string& app_id) const = 0;

  // These functions are used to transfer ownership of the lazily initialized
  // context from ProfileIOData to the URLRequestContextGetter.
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireMediaRequestContext() const = 0;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireIsolatedAppRequestContext(
          scoped_refptr<ChromeURLRequestContext> main_context,
          const std::string& app_id) const = 0;

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the Profile, used to initialize ProfileIOData.
  // Deleted after lazy initialization.
  mutable scoped_ptr<ProfileParams> profile_params_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember enable_referrers_;
  mutable BooleanPrefMember clear_local_state_on_exit_;
  mutable BooleanPrefMember safe_browsing_enabled_;

  // Pointed to by NetworkDelegate.
  mutable scoped_ptr<policy::URLBlacklistManager> url_blacklist_manager_;

  // Pointed to by URLRequestContext.
  mutable scoped_ptr<ChromeURLDataManagerBackend>
      chrome_url_data_manager_backend_;
  mutable scoped_ptr<net::OriginBoundCertService> origin_bound_cert_service_;
  mutable scoped_ptr<net::NetworkDelegate> network_delegate_;
  mutable scoped_ptr<net::DnsCertProvenanceChecker> dns_cert_checker_;
  mutable scoped_ptr<net::FraudulentCertificateReporter>
      fraudulent_certificate_reporter_;
  mutable scoped_ptr<net::ProxyService> proxy_service_;
  mutable scoped_ptr<net::TransportSecurityState> transport_security_state_;
  mutable scoped_ptr<net::URLRequestJobFactory> job_factory_;

  // Pointed to by ResourceContext.
  mutable scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;
  mutable scoped_refptr<ChromeAppCacheService> appcache_service_;
  mutable scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  mutable scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  mutable scoped_refptr<quota::QuotaManager> quota_manager_;
  mutable scoped_refptr<HostZoomMap> host_zoom_map_;
  mutable scoped_refptr<DownloadIdFactory> download_id_factory_;
  mutable scoped_ptr<media_stream::MediaStreamManager> media_stream_manager_;

  // TODO(willchan): Remove from ResourceContext.
  mutable scoped_refptr<ExtensionInfoMap> extension_info_map_;
  mutable scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  mutable scoped_refptr<CookieSettings> cookie_settings_;
  mutable DesktopNotificationService* notification_service_;

  mutable ResourceContext resource_context_;

  mutable scoped_ptr<TransportSecurityPersister>
      transport_security_persister_;

  // These are only valid in between LazyInitialize() and their accessor being
  // called.
  mutable scoped_refptr<ChromeURLRequestContext> main_request_context_;
  mutable scoped_refptr<ChromeURLRequestContext> extensions_request_context_;
  // One AppRequestContext per isolated app.
  mutable AppRequestContextMap app_request_context_map_;

  // TODO(jhawkins): Remove once crbug.com/102004 is fixed.
  bool initialized_on_UI_thread_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

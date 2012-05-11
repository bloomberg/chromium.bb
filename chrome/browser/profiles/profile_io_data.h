// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/resource_context.h"
#include "net/cookies/cookie_monster.h"

class CookieSettings;
class DesktopNotificationService;
class ExtensionInfoMap;
class HostContentSettingsMap;
class IOThread;
class Profile;
class ProtocolHandlerRegistry;
class TransportSecurityPersister;

namespace chrome_browser_net {
class HttpServerPropertiesManager;
}

namespace net {
class CookieStore;
class FraudulentCertificateReporter;
class HttpTransactionFactory;
class ServerBoundCertService;
class ProxyConfigService;
class ProxyService;
class SSLConfigService;
class TransportSecurityState;
}  // namespace net

namespace policy {
class URLBlacklistManager;
}  // namespace policy

// Conceptually speaking, the ProfileIOData represents data that lives on the IO
// thread that is owned by a Profile, such as, but not limited to, network
// objects like CookieMonster, HttpTransactionFactory, etc.  Profile owns
// ProfileIOData, but will make sure to delete it on the IO thread (except
// possibly in unit tests where there is no IO thread).
class ProfileIOData {
 public:
  virtual ~ProfileIOData();

  static ProfileIOData* FromResourceContext(content::ResourceContext* rc);

  // Returns true if |scheme| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if |url| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledURL(const GURL& url);

  // Called by Profile.
  content::ResourceContext* GetResourceContext() const;
  ChromeURLDataManagerBackend* GetChromeURLDataManagerBackend() const;

  // These should only be called at most once each. Ownership is reversed when
  // they get called, from ProfileIOData owning ChromeURLRequestContext to vice
  // versa.
  ChromeURLRequestContext* GetMainRequestContext() const;
  ChromeURLRequestContext* GetMediaRequestContext() const;
  ChromeURLRequestContext* GetExtensionsRequestContext() const;
  ChromeURLRequestContext* GetIsolatedAppRequestContext(
      ChromeURLRequestContext* main_context,
      const std::string& app_id) const;

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that profile.
  ExtensionInfoMap* GetExtensionInfoMap() const;
  CookieSettings* GetCookieSettings() const;

#if defined(ENABLE_NOTIFICATIONS)
  DesktopNotificationService* GetNotificationService() const;
#endif

  BooleanPrefMember* clear_local_state_on_exit()  const {
    return &clear_local_state_on_exit_;
  }

  IntegerPrefMember* session_startup_pref() const {
    return &session_startup_pref_;
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

  chrome_browser_net::HttpServerPropertiesManager*
      http_server_properties_manager() const;

  bool is_incognito() const {
    return is_incognito_;
  }

  // Initialize the member needed to track the metrics enabled state. This is
  // only to be called on the UI thread.
  void InitializeMetricsEnabledStateOnUIThread();

  // Returns whether or not metrics reporting is enabled in the browser instance
  // on which this profile resides. This is safe for use from the IO thread, and
  // should only be called from there.
  bool GetMetricsEnabledStateOnIOThread() const;

 protected:
  class AppRequestContext : public ChromeURLRequestContext {
   public:
    AppRequestContext();

    void SetCookieStore(net::CookieStore* cookie_store);
    void SetHttpTransactionFactory(net::HttpTransactionFactory* http_factory);

   private:
    virtual ~AppRequestContext();

    scoped_refptr<net::CookieStore> cookie_store_;
    scoped_ptr<net::HttpTransactionFactory> http_factory_;
  };

  // Created on the UI thread, read on the IO thread during ProfileIOData lazy
  // initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    FilePath path;
    bool clear_local_state_on_exit;
    std::string accept_language;
    std::string accept_charset;
    std::string referrer_charset;
    IOThread* io_thread;
    scoped_refptr<CookieSettings> cookie_settings;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_refptr<net::CookieMonster::Delegate> cookie_monster_delegate;
    scoped_refptr<ExtensionInfoMap> extension_info_map;

#if defined(ENABLE_NOTIFICATIONS)
    DesktopNotificationService* notification_service;
#endif

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

  static std::string GetSSLSessionCacheShard();

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

  // A ServerBoundCertService object is created by a derived class of
  // ProfileIOData, and the derived class calls this method to set the
  // server_bound_cert_service_ member and transfers ownership to the base
  // class.
  void set_server_bound_cert_service(
      net::ServerBoundCertService* server_bound_cert_service) const;

  net::NetworkDelegate* network_delegate() const {
    return network_delegate_.get();
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

  void set_http_server_properties_manager(
      chrome_browser_net::HttpServerPropertiesManager* manager) const;

  ChromeURLRequestContext* main_request_context() const {
    return main_request_context_.get();
  }

  // Destroys the ResourceContext first, to cancel any URLRequests that are
  // using it still, before we destroy the member variables that those
  // URLRequests may be accessing.
  void DestroyResourceContext();

 private:
  class ResourceContext : public content::ResourceContext {
   public:
    explicit ResourceContext(ProfileIOData* io_data);
    virtual ~ResourceContext();

    // ResourceContext implementation:
    virtual net::HostResolver* GetHostResolver() OVERRIDE;
    virtual net::URLRequestContext* GetRequestContext() OVERRIDE;

   private:
    friend class ProfileIOData;

    void EnsureInitialized();

    ProfileIOData* const io_data_;

    net::HostResolver* host_resolver_;
    net::URLRequestContext* request_context_;
  };

  typedef base::hash_map<std::string, ChromeURLRequestContext*>
      AppRequestContextMap;

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does the actual initialization of the ProfileIOData subtype. Subtypes
  // should use the static helper functions above to implement this.
  virtual void LazyInitializeInternal(ProfileParams* profile_params) const = 0;

  // Does an on-demand initialization of a RequestContext for the given
  // isolated app.
  virtual ChromeURLRequestContext* InitializeAppRequestContext(
      ChromeURLRequestContext* main_context,
      const std::string& app_id) const = 0;

  // These functions are used to transfer ownership of the lazily initialized
  // context from ProfileIOData to the URLRequestContextGetter.
  virtual ChromeURLRequestContext*
      AcquireMediaRequestContext() const = 0;
  virtual ChromeURLRequestContext*
      AcquireIsolatedAppRequestContext(
          ChromeURLRequestContext* main_context,
          const std::string& app_id) const = 0;

  // The order *DOES* matter for the majority of these member variables, so
  // don't move them around unless you know what you're doing!
  // General rules:
  //   * ResourceContext references the URLRequestContexts, so
  //   URLRequestContexts must outlive ResourceContext, hence ResourceContext
  //   should be destroyed first.
  //   * URLRequestContexts reference a whole bunch of members, so
  //   URLRequestContext needs to be destroyed before them.
  //   * Therefore, ResourceContext should be listed last, and then the
  //   URLRequestContexts, and then the URLRequestContext members.
  //   * Note that URLRequestContext members have a directed dependency graph
  //   too, so they must themselves be ordered correctly.

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the Profile, used to initialize ProfileIOData.
  // Deleted after lazy initialization.
  mutable scoped_ptr<ProfileParams> profile_params_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember enable_referrers_;
  mutable BooleanPrefMember clear_local_state_on_exit_;
  mutable BooleanPrefMember safe_browsing_enabled_;
  // TODO(marja): Remove session_startup_pref_ if no longer needed.
  mutable IntegerPrefMember session_startup_pref_;

  // The state of metrics reporting in the browser that this profile runs on.
  // Unfortunately, since ChromeOS has a separate representation of this state,
  // we need to make one available based on the platform.
#if defined(OS_CHROMEOS)
  bool enable_metrics_;
#else
  BooleanPrefMember enable_metrics_;
#endif

  // Pointed to by NetworkDelegate.
  mutable scoped_ptr<policy::URLBlacklistManager> url_blacklist_manager_;

  // Pointed to by URLRequestContext.
  mutable scoped_ptr<ChromeURLDataManagerBackend>
      chrome_url_data_manager_backend_;
  mutable scoped_ptr<net::ServerBoundCertService> server_bound_cert_service_;
  mutable scoped_ptr<net::NetworkDelegate> network_delegate_;
  mutable scoped_ptr<net::FraudulentCertificateReporter>
      fraudulent_certificate_reporter_;
  mutable scoped_ptr<net::ProxyService> proxy_service_;
  mutable scoped_ptr<net::TransportSecurityState> transport_security_state_;
  mutable scoped_ptr<net::URLRequestJobFactory> job_factory_;
  mutable scoped_ptr<chrome_browser_net::HttpServerPropertiesManager>
      http_server_properties_manager_;

#if defined(ENABLE_NOTIFICATIONS)
  mutable DesktopNotificationService* notification_service_;
#endif

  mutable scoped_ptr<TransportSecurityPersister>
      transport_security_persister_;

  // These are only valid in between LazyInitialize() and their accessor being
  // called.
  mutable scoped_ptr<ChromeURLRequestContext> main_request_context_;
  mutable scoped_ptr<ChromeURLRequestContext> extensions_request_context_;
  // One AppRequestContext per isolated app.
  mutable AppRequestContextMap app_request_context_map_;

  mutable scoped_ptr<ResourceContext> resource_context_;

  mutable scoped_refptr<ExtensionInfoMap> extension_info_map_;
  mutable scoped_refptr<CookieSettings> cookie_settings_;

  // TODO(jhawkins): Remove once crbug.com/102004 is fixed.
  bool initialized_on_UI_thread_;

  bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

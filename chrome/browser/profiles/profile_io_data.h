// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_member.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "content/public/browser/resource_context.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_network_session.h"
#include "net/url_request/url_request_job_factory.h"

class ChromeHttpUserAgentSettings;
class ChromeNetworkDelegate;
class CookieSettings;
class DesktopNotificationService;
class ExtensionInfoMap;
class HostContentSettingsMap;
class ManagedModeURLFilter;
class Profile;
class ProtocolHandlerRegistry;
class SigninNamesOnIOThread;
class TransportSecurityPersister;

namespace chrome_browser_net {
class LoadTimeStats;
class ResourcePrefetchPredictorObserver;
}

namespace net {
class CookieStore;
class FraudulentCertificateReporter;
class HttpServerProperties;
class HttpTransactionFactory;
class ServerBoundCertService;
class ProxyConfigService;
class ProxyService;
class SSLConfigService;
class TransportSecurityState;
class URLRequestJobFactoryImpl;
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
      const StoragePartitionDescriptor& partition_descriptor,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor) const;
  ChromeURLRequestContext* GetIsolatedMediaRequestContext(
      ChromeURLRequestContext* app_context,
      const StoragePartitionDescriptor& partition_descriptor) const;

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that profile.
  ExtensionInfoMap* GetExtensionInfoMap() const;
  CookieSettings* GetCookieSettings() const;

#if defined(ENABLE_NOTIFICATIONS)
  DesktopNotificationService* GetNotificationService() const;
#endif

  IntegerPrefMember* session_startup_pref() const {
    return &session_startup_pref_;
  }

  SigninNamesOnIOThread* signin_names() const {
    return signin_names_.get();
  }

  StringPrefMember* google_services_username() const {
    return &google_services_username_;
  }

  StringPrefMember* google_services_username_pattern() const {
    return &google_services_username_pattern_;
  }

  BooleanPrefMember* reverse_autologin_enabled() const {
    return &reverse_autologin_enabled_;
  }

  StringListPrefMember* one_click_signin_rejected_email_list() const {
    return &one_click_signin_rejected_email_list_;
  }

  ChromeURLRequestContext* extensions_request_context() const {
    return extensions_request_context_.get();
  }

  BooleanPrefMember* safe_browsing_enabled() const {
    return &safe_browsing_enabled_;
  }

  BooleanPrefMember* printing_enabled() const {
    return &printing_enabled_;
  }

  BooleanPrefMember* sync_disabled() const {
    return &sync_disabled_;
  }

  net::TransportSecurityState* transport_security_state() const {
    return transport_security_state_.get();
  }

  bool is_incognito() const {
    return is_incognito_;
  }

  chrome_browser_net::ResourcePrefetchPredictorObserver*
      resource_prefetch_predictor_observer() const {
    return resource_prefetch_predictor_observer_.get();
  }

#if defined(ENABLE_MANAGED_USERS)
  const ManagedModeURLFilter* managed_mode_url_filter() const {
    return managed_mode_url_filter_.get();
  }
#endif

  // Initialize the member needed to track the metrics enabled state. This is
  // only to be called on the UI thread.
  void InitializeMetricsEnabledStateOnUIThread();

  // Returns whether or not metrics reporting is enabled in the browser instance
  // on which this profile resides. This is safe for use from the IO thread, and
  // should only be called from there.
  bool GetMetricsEnabledStateOnIOThread() const;

 protected:
  // A URLRequestContext for media that owns its HTTP factory, to ensure
  // it is deleted.
  class MediaRequestContext : public ChromeURLRequestContext {
   public:
    explicit MediaRequestContext(
        chrome_browser_net::LoadTimeStats* load_time_stats);

    void SetHttpTransactionFactory(
        scoped_ptr<net::HttpTransactionFactory> http_factory);

   private:
    virtual ~MediaRequestContext();

    scoped_ptr<net::HttpTransactionFactory> http_factory_;
  };

  // A URLRequestContext for apps that owns its cookie store and HTTP factory,
  // to ensure they are deleted.
  class AppRequestContext : public ChromeURLRequestContext {
   public:
    explicit AppRequestContext(
        chrome_browser_net::LoadTimeStats* load_time_stats);

    void SetCookieStore(net::CookieStore* cookie_store);
    void SetHttpTransactionFactory(
        scoped_ptr<net::HttpTransactionFactory> http_factory);
    void SetJobFactory(scoped_ptr<net::URLRequestJobFactory> job_factory);

   private:
    virtual ~AppRequestContext();

    scoped_refptr<net::CookieStore> cookie_store_;
    scoped_ptr<net::HttpTransactionFactory> http_factory_;
    scoped_ptr<net::URLRequestJobFactory> job_factory_;
  };

  // Created on the UI thread, read on the IO thread during ProfileIOData lazy
  // initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    FilePath path;
    IOThread* io_thread;
    scoped_refptr<CookieSettings> cookie_settings;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_refptr<net::CookieMonster::Delegate> cookie_monster_delegate;
    scoped_refptr<ExtensionInfoMap> extension_info_map;
    scoped_ptr<chrome_browser_net::ResourcePrefetchPredictorObserver>
        resource_prefetch_predictor_observer_;

#if defined(ENABLE_NOTIFICATIONS)
    DesktopNotificationService* notification_service;
#endif

    // This pointer exists only as a means of conveying a url job factory
    // pointer from the protocol handler registry on the UI thread to the
    // the URLRequestContext on the IO thread. The consumer MUST take
    // ownership of the object by calling release() on this pointer.
    scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor;

    // We need to initialize the ProxyConfigService from the UI thread
    // because on linux it relies on initializing things through gconf,
    // and needs to be on the main thread.
    scoped_ptr<net::ProxyConfigService> proxy_config_service;

#if defined(ENABLE_MANAGED_USERS)
    scoped_refptr<const ManagedModeURLFilter> managed_mode_url_filter;
#endif

    // The profile this struct was populated from. It's passed as a void* to
    // ensure it's not accidently used on the IO thread. Before using it on the
    // UI thread, call ProfileManager::IsValidProfile to ensure it's alive.
    void* profile;
  };

  explicit ProfileIOData(bool is_incognito);

  static std::string GetSSLSessionCacheShard();

  void InitializeOnUIThread(Profile* profile);
  void ApplyProfileParamsToContext(ChromeURLRequestContext* context) const;

  scoped_ptr<net::URLRequestJobFactory> SetUpJobFactoryDefaults(
      scoped_ptr<net::URLRequestJobFactoryImpl> job_factory,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor,
      net::NetworkDelegate* network_delegate,
      net::FtpTransactionFactory* ftp_transaction_factory,
      net::FtpAuthCache* ftp_auth_cache) const;

  // Lazy initializes the ProfileIOData object the first time a request context
  // is requested. The lazy logic is implemented here. The actual initialization
  // is done in LazyInitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  void LazyInitialize() const;

  // Called when the profile is destroyed.
  void ShutdownOnUIThread();

  ChromeURLDataManagerBackend* chrome_url_data_manager_backend() const {
    return chrome_url_data_manager_backend_.get();
  }

  // A ServerBoundCertService object is created by a derived class of
  // ProfileIOData, and the derived class calls this method to set the
  // server_bound_cert_service_ member and transfers ownership to the base
  // class.
  void set_server_bound_cert_service(
      net::ServerBoundCertService* server_bound_cert_service) const;

  ChromeNetworkDelegate* network_delegate() const {
    return network_delegate_.get();
  }

  net::FraudulentCertificateReporter* fraudulent_certificate_reporter() const {
    return fraudulent_certificate_reporter_.get();
  }

  net::ProxyService* proxy_service() const {
    return proxy_service_.get();
  }

  net::HttpServerProperties* http_server_properties() const;

  void set_http_server_properties(
      net::HttpServerProperties* http_server_properties) const;

  ChromeURLRequestContext* main_request_context() const {
    return main_request_context_.get();
  }

  chrome_browser_net::LoadTimeStats* load_time_stats() const {
    return load_time_stats_;
  }

  // Destroys the ResourceContext first, to cancel any URLRequests that are
  // using it still, before we destroy the member variables that those
  // URLRequests may be accessing.
  void DestroyResourceContext();

  // Fills in fields of params using values from main_request_context_ and the
  // IOThread associated with profile_params.
  void PopulateNetworkSessionParams(
      const ProfileParams* profile_params,
      net::HttpNetworkSession::Params* params) const;

  void SetCookieSettingsForTesting(CookieSettings* cookie_settings);

  void set_signin_names_for_testing(SigninNamesOnIOThread* signin_names);

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

  typedef std::map<StoragePartitionDescriptor,
                   ChromeURLRequestContext*,
                   StoragePartitionDescriptorLess>
      URLRequestContextMap;

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does the actual initialization of the ProfileIOData subtype. Subtypes
  // should use the static helper functions above to implement this.
  virtual void LazyInitializeInternal(ProfileParams* profile_params) const = 0;

  // Initializes the RequestContext for extensions.
  virtual void InitializeExtensionsRequestContext(
      ProfileParams* profile_params) const = 0;
  // Does an on-demand initialization of a RequestContext for the given
  // isolated app.
  virtual ChromeURLRequestContext* InitializeAppRequestContext(
      ChromeURLRequestContext* main_context,
      const StoragePartitionDescriptor& details,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor) const = 0;

  // Does an on-demand initialization of a media RequestContext for the given
  // isolated app.
  virtual ChromeURLRequestContext* InitializeMediaRequestContext(
      ChromeURLRequestContext* original_context,
      const StoragePartitionDescriptor& details) const = 0;

  // These functions are used to transfer ownership of the lazily initialized
  // context from ProfileIOData to the URLRequestContextGetter.
  virtual ChromeURLRequestContext*
      AcquireMediaRequestContext() const = 0;
  virtual ChromeURLRequestContext*
      AcquireIsolatedAppRequestContext(
          ChromeURLRequestContext* main_context,
          const StoragePartitionDescriptor& partition_descriptor,
          scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
              protocol_handler_interceptor) const = 0;
  virtual ChromeURLRequestContext*
      AcquireIsolatedMediaRequestContext(
          ChromeURLRequestContext* app_context,
          const StoragePartitionDescriptor& partition_descriptor) const = 0;

  // Returns the LoadTimeStats object to be used for this profile.
  virtual chrome_browser_net::LoadTimeStats* GetLoadTimeStats(
      IOThread::Globals* io_thread_globals) const = 0;

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

  // Provides access to the email addresses of all signed in profiles.
  mutable scoped_ptr<SigninNamesOnIOThread> signin_names_;

  mutable StringPrefMember google_services_username_;
  mutable StringPrefMember google_services_username_pattern_;
  mutable BooleanPrefMember reverse_autologin_enabled_;
  mutable StringListPrefMember one_click_signin_rejected_email_list_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember enable_referrers_;
  mutable BooleanPrefMember enable_do_not_track_;
  mutable BooleanPrefMember force_safesearch_;
  mutable BooleanPrefMember safe_browsing_enabled_;
  mutable BooleanPrefMember printing_enabled_;
  mutable BooleanPrefMember sync_disabled_;
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
  mutable scoped_refptr<ExtensionInfoMap> extension_info_map_;
  mutable scoped_ptr<ChromeURLDataManagerBackend>
      chrome_url_data_manager_backend_;
  mutable scoped_ptr<net::ServerBoundCertService> server_bound_cert_service_;
  mutable scoped_ptr<ChromeNetworkDelegate> network_delegate_;
  mutable scoped_ptr<net::FraudulentCertificateReporter>
      fraudulent_certificate_reporter_;
  mutable scoped_ptr<net::ProxyService> proxy_service_;
  mutable scoped_ptr<net::TransportSecurityState> transport_security_state_;
  mutable scoped_ptr<net::HttpServerProperties>
      http_server_properties_;

#if defined(ENABLE_NOTIFICATIONS)
  mutable DesktopNotificationService* notification_service_;
#endif

  mutable scoped_ptr<TransportSecurityPersister>
      transport_security_persister_;

  // These are only valid in between LazyInitialize() and their accessor being
  // called.
  mutable scoped_ptr<ChromeURLRequestContext> main_request_context_;
  mutable scoped_ptr<ChromeURLRequestContext> extensions_request_context_;
  // One URLRequestContext per isolated app for main and media requests.
  mutable URLRequestContextMap app_request_context_map_;
  mutable URLRequestContextMap isolated_media_request_context_map_;

  mutable scoped_ptr<ResourceContext> resource_context_;

  mutable scoped_refptr<CookieSettings> cookie_settings_;

  mutable scoped_ptr<chrome_browser_net::ResourcePrefetchPredictorObserver>
      resource_prefetch_predictor_observer_;

  mutable scoped_ptr<ChromeHttpUserAgentSettings>
      chrome_http_user_agent_settings_;

  mutable chrome_browser_net::LoadTimeStats* load_time_stats_;

#if defined(ENABLE_MANAGED_USERS)
  mutable scoped_refptr<const ManagedModeURLFilter> managed_mode_url_filter_;
#endif

  // TODO(jhawkins): Remove once crbug.com/102004 is fixed.
  bool initialized_on_UI_thread_;

  bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

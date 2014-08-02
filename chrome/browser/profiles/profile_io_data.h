// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"

class ChromeHttpUserAgentSettings;
class ChromeNetworkDelegate;
class CookieSettings;
class DevToolsNetworkController;
class HostContentSettingsMap;
class MediaDeviceIDSalt;
class ProtocolHandlerRegistry;
class SigninNamesOnIOThread;
class SupervisedUserURLFilter;

namespace extensions {
class InfoMap;
}

namespace net {
class ChannelIDService;
class CookieStore;
class FraudulentCertificateReporter;
class FtpTransactionFactory;
class HttpServerProperties;
class HttpTransactionFactory;
class ProxyConfigService;
class ProxyService;
class SSLConfigService;
class TransportSecurityPersister;
class TransportSecurityState;
class URLRequestJobFactoryImpl;
}  // namespace net

namespace policy {
class PolicyCertVerifier;
class PolicyHeaderIOHelper;
class URLBlacklistManager;
}  // namespace policy

namespace prerender {
class PrerenderTracker;
}

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

  // Utility to install additional WebUI handlers into the |job_factory|.
  // Ownership of the handlers is transfered from |protocol_handlers|
  // to the |job_factory|.
  static void InstallProtocolHandlers(
      net::URLRequestJobFactoryImpl* job_factory,
      content::ProtocolHandlerMap* protocol_handlers);

  // Called by Profile.
  content::ResourceContext* GetResourceContext() const;

  // Initializes the ProfileIOData object and primes the RequestContext
  // generation. Must be called prior to any of the Get*() methods other than
  // GetResouceContext or GetMetricsEnabledStateOnIOThread.
  void Init(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) const;

  net::URLRequestContext* GetMainRequestContext() const;
  net::URLRequestContext* GetMediaRequestContext() const;
  net::URLRequestContext* GetExtensionsRequestContext() const;
  net::URLRequestContext* GetIsolatedAppRequestContext(
      net::URLRequestContext* main_context,
      const StoragePartitionDescriptor& partition_descriptor,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) const;
  net::URLRequestContext* GetIsolatedMediaRequestContext(
      net::URLRequestContext* app_context,
      const StoragePartitionDescriptor& partition_descriptor) const;

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that profile.
  extensions::InfoMap* GetExtensionInfoMap() const;
  CookieSettings* GetCookieSettings() const;
  HostContentSettingsMap* GetHostContentSettingsMap() const;

  IntegerPrefMember* session_startup_pref() const {
    return &session_startup_pref_;
  }

  SigninNamesOnIOThread* signin_names() const {
    return signin_names_.get();
  }

  StringPrefMember* google_services_account_id() const {
    return &google_services_user_account_id_;
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

  const std::string& reverse_autologin_pending_email() const {
    return reverse_autologin_pending_email_;
  }

  void set_reverse_autologin_pending_email(const std::string& email) {
    reverse_autologin_pending_email_ = email;
  }

  StringListPrefMember* one_click_signin_rejected_email_list() const {
    return &one_click_signin_rejected_email_list_;
  }

  net::URLRequestContext* extensions_request_context() const {
    return extensions_request_context_.get();
  }

  BooleanPrefMember* safe_browsing_enabled() const {
    return &safe_browsing_enabled_;
  }

#if defined(OS_ANDROID)
  // TODO(feng): move the function to protected area.
  // IsDataReductionProxyEnabled() should be used as public API.
  BooleanPrefMember* data_reduction_proxy_enabled() const {
    return &data_reduction_proxy_enabled_;
  }
#endif

  BooleanPrefMember* printing_enabled() const {
    return &printing_enabled_;
  }

  BooleanPrefMember* sync_disabled() const {
    return &sync_disabled_;
  }

  BooleanPrefMember* signin_allowed() const {
    return &signin_allowed_;
  }

  // TODO(bnc): remove per https://crbug.com/334602.
  BooleanPrefMember* network_prediction_enabled() const {
    return &network_prediction_enabled_;
  }

  IntegerPrefMember* network_prediction_options() const {
    return &network_prediction_options_;
  }

  content::ResourceContext::SaltCallback GetMediaDeviceIDSalt() const;

  DevToolsNetworkController* network_controller() const {
    return network_controller_.get();
  }

  net::TransportSecurityState* transport_security_state() const {
    return transport_security_state_.get();
  }

#if defined(OS_CHROMEOS)
  std::string username_hash() const {
    return username_hash_;
  }

  bool use_system_key_slot() const { return use_system_key_slot_; }
#endif

  Profile::ProfileType profile_type() const {
    return profile_type_;
  }

  bool IsOffTheRecord() const;

  IntegerPrefMember* incognito_availibility() const {
    return &incognito_availibility_pref_;
  }

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::PolicyHeaderIOHelper* policy_header_helper() const {
    return policy_header_helper_.get();
  }
#endif

#if defined(ENABLE_MANAGED_USERS)
  const SupervisedUserURLFilter* supervised_user_url_filter() const {
    return supervised_user_url_filter_.get();
  }
#endif

  // Initialize the member needed to track the metrics enabled state. This is
  // only to be called on the UI thread.
  void InitializeMetricsEnabledStateOnUIThread();

  // Returns whether or not metrics reporting is enabled in the browser instance
  // on which this profile resides. This is safe for use from the IO thread, and
  // should only be called from there.
  bool GetMetricsEnabledStateOnIOThread() const;

#if defined(OS_ANDROID)
  // Returns whether or not data reduction proxy is enabled in the browser
  // instance on which this profile resides.
  bool IsDataReductionProxyEnabled() const;
#endif

  void set_client_cert_store_factory_for_testing(
    const base::Callback<scoped_ptr<net::ClientCertStore>()>& factory) {
      client_cert_store_factory_ = factory;
  }

 protected:
  // A URLRequestContext for media that owns its HTTP factory, to ensure
  // it is deleted.
  class MediaRequestContext : public net::URLRequestContext {
   public:
    MediaRequestContext();

    void SetHttpTransactionFactory(
        scoped_ptr<net::HttpTransactionFactory> http_factory);

   private:
    virtual ~MediaRequestContext();

    scoped_ptr<net::HttpTransactionFactory> http_factory_;
  };

  // A URLRequestContext for apps that owns its cookie store and HTTP factory,
  // to ensure they are deleted.
  class AppRequestContext : public net::URLRequestContext {
   public:
    AppRequestContext();

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

    base::FilePath path;
    IOThread* io_thread;
    scoped_refptr<CookieSettings> cookie_settings;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_refptr<net::CookieMonster::Delegate> cookie_monster_delegate;
#if defined(ENABLE_EXTENSIONS)
    scoped_refptr<extensions::InfoMap> extension_info_map;
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
    scoped_refptr<const SupervisedUserURLFilter> supervised_user_url_filter;
#endif

#if defined(OS_CHROMEOS)
    std::string username_hash;
    bool use_system_key_slot;
#endif

    // The profile this struct was populated from. It's passed as a void* to
    // ensure it's not accidently used on the IO thread. Before using it on the
    // UI thread, call ProfileManager::IsValidProfile to ensure it's alive.
    void* profile;

    prerender::PrerenderTracker* prerender_tracker;
  };

  explicit ProfileIOData(Profile::ProfileType profile_type);

  static std::string GetSSLSessionCacheShard();

  void InitializeOnUIThread(Profile* profile);
  void ApplyProfileParamsToContext(net::URLRequestContext* context) const;

#if defined(OS_ANDROID)
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  void SetDataReductionProxyUsageStatsOnIOThread(IOThread* io_thread,
                                                 Profile* profile);
  void SetDataReductionProxyUsageStatsOnUIThread(Profile* profile,
      data_reduction_proxy::DataReductionProxyUsageStats* usage_stats);
#endif
#endif

  scoped_ptr<net::URLRequestJobFactory> SetUpJobFactoryDefaults(
      scoped_ptr<net::URLRequestJobFactoryImpl> job_factory,
      content::URLRequestInterceptorScopedVector request_interceptors,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor,
      net::NetworkDelegate* network_delegate,
      net::FtpTransactionFactory* ftp_transaction_factory) const;

  // Called when the profile is destroyed.
  void ShutdownOnUIThread();

  // A ChannelIDService object is created by a derived class of
  // ProfileIOData, and the derived class calls this method to set the
  // channel_id_service_ member and transfers ownership to the base
  // class.
  void set_channel_id_service(
      net::ChannelIDService* channel_id_service) const;

  ChromeNetworkDelegate* network_delegate() const {
    return network_delegate_.get();
  }

  net::FraudulentCertificateReporter* fraudulent_certificate_reporter() const {
    return fraudulent_certificate_reporter_.get();
  }

  net::ProxyService* proxy_service() const {
    return proxy_service_.get();
  }

  base::WeakPtr<net::HttpServerProperties> http_server_properties() const;

  void set_http_server_properties(
      scoped_ptr<net::HttpServerProperties> http_server_properties) const;

  net::URLRequestContext* main_request_context() const {
    return main_request_context_.get();
  }

  bool initialized() const {
    return initialized_;
  }

  // Destroys the ResourceContext first, to cancel any URLRequests that are
  // using it still, before we destroy the member variables that those
  // URLRequests may be accessing.
  void DestroyResourceContext();

  // Creates network session and main network transaction factory.
  scoped_ptr<net::HttpCache> CreateMainHttpFactory(
      const ProfileParams* profile_params,
      net::HttpCache::BackendFactory* main_backend) const;

  // Creates network transaction factory.
  scoped_ptr<net::HttpCache> CreateHttpFactory(
      net::HttpNetworkSession* shared_session,
      net::HttpCache::BackendFactory* backend) const;

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
    virtual scoped_ptr<net::ClientCertStore> CreateClientCertStore() OVERRIDE;
    virtual void CreateKeygenHandler(
        uint32 key_size_in_bits,
        const std::string& challenge_string,
        const GURL& url,
        const base::Callback<void(scoped_ptr<net::KeygenHandler>)>& callback)
        OVERRIDE;
    virtual bool AllowMicAccess(const GURL& origin) OVERRIDE;
    virtual bool AllowCameraAccess(const GURL& origin) OVERRIDE;
    virtual SaltCallback GetMediaDeviceIDSalt() OVERRIDE;

   private:
    friend class ProfileIOData;

    // Helper method that returns true if |type| is allowed for |origin|, false
    // otherwise.
    bool AllowContentAccess(const GURL& origin, ContentSettingsType type);

    ProfileIOData* const io_data_;

    net::HostResolver* host_resolver_;
    net::URLRequestContext* request_context_;
  };

  typedef std::map<StoragePartitionDescriptor,
                   net::URLRequestContext*,
                   StoragePartitionDescriptorLess>
      URLRequestContextMap;

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does the actual initialization of the ProfileIOData subtype. Subtypes
  // should use the static helper functions above to implement this.
  virtual void InitializeInternal(
      ProfileParams* profile_params,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector
          request_interceptors) const = 0;

  // Initializes the RequestContext for extensions.
  virtual void InitializeExtensionsRequestContext(
      ProfileParams* profile_params) const = 0;
  // Does an on-demand initialization of a RequestContext for the given
  // isolated app.
  virtual net::URLRequestContext* InitializeAppRequestContext(
      net::URLRequestContext* main_context,
      const StoragePartitionDescriptor& details,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector
          request_interceptors) const = 0;

  // Does an on-demand initialization of a media RequestContext for the given
  // isolated app.
  virtual net::URLRequestContext* InitializeMediaRequestContext(
      net::URLRequestContext* original_context,
      const StoragePartitionDescriptor& details) const = 0;

  // These functions are used to transfer ownership of the lazily initialized
  // context from ProfileIOData to the URLRequestContextGetter.
  virtual net::URLRequestContext*
      AcquireMediaRequestContext() const = 0;
  virtual net::URLRequestContext* AcquireIsolatedAppRequestContext(
      net::URLRequestContext* main_context,
      const StoragePartitionDescriptor& partition_descriptor,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector
          request_interceptors) const = 0;
  virtual net::URLRequestContext*
      AcquireIsolatedMediaRequestContext(
          net::URLRequestContext* app_context,
          const StoragePartitionDescriptor& partition_descriptor) const = 0;

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

  // Used for testing.
  mutable base::Callback<scoped_ptr<net::ClientCertStore>()>
      client_cert_store_factory_;

  mutable StringPrefMember google_services_user_account_id_;
  mutable StringPrefMember google_services_username_;
  mutable StringPrefMember google_services_username_pattern_;
  mutable BooleanPrefMember reverse_autologin_enabled_;

  // During the reverse autologin request chain processing, this member saves
  // the email of the google account that is being signed into.
  std::string reverse_autologin_pending_email_;

  mutable StringListPrefMember one_click_signin_rejected_email_list_;

  mutable scoped_refptr<MediaDeviceIDSalt> media_device_id_salt_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember enable_referrers_;
  mutable BooleanPrefMember enable_do_not_track_;
  mutable BooleanPrefMember force_safesearch_;
  mutable BooleanPrefMember safe_browsing_enabled_;
#if defined(OS_ANDROID)
  mutable BooleanPrefMember data_reduction_proxy_enabled_;
#endif
  mutable BooleanPrefMember printing_enabled_;
  mutable BooleanPrefMember sync_disabled_;
  mutable BooleanPrefMember signin_allowed_;
  mutable BooleanPrefMember network_prediction_enabled_;
  mutable IntegerPrefMember network_prediction_options_;
  // TODO(marja): Remove session_startup_pref_ if no longer needed.
  mutable IntegerPrefMember session_startup_pref_;
  mutable BooleanPrefMember quick_check_enabled_;
  mutable IntegerPrefMember incognito_availibility_pref_;

  // The state of metrics reporting in the browser that this profile runs on.
  // Unfortunately, since ChromeOS has a separate representation of this state,
  // we need to make one available based on the platform.
#if defined(OS_CHROMEOS)
  bool enable_metrics_;
#else
  BooleanPrefMember enable_metrics_;
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
  // Pointed to by NetworkDelegate.
  mutable scoped_ptr<policy::URLBlacklistManager> url_blacklist_manager_;
  mutable scoped_ptr<policy::PolicyHeaderIOHelper> policy_header_helper_;
#endif

  // Pointed to by URLRequestContext.
#if defined(ENABLE_EXTENSIONS)
  mutable scoped_refptr<extensions::InfoMap> extension_info_map_;
#endif
  mutable scoped_ptr<net::ChannelIDService> channel_id_service_;
  mutable scoped_ptr<ChromeNetworkDelegate> network_delegate_;
  mutable scoped_ptr<net::FraudulentCertificateReporter>
      fraudulent_certificate_reporter_;
  mutable scoped_ptr<net::ProxyService> proxy_service_;
  mutable scoped_ptr<net::TransportSecurityState> transport_security_state_;
  mutable scoped_ptr<net::HttpServerProperties>
      http_server_properties_;
#if defined(OS_CHROMEOS)
  mutable scoped_ptr<policy::PolicyCertVerifier> cert_verifier_;
  mutable std::string username_hash_;
  mutable bool use_system_key_slot_;
#endif

  mutable scoped_ptr<net::TransportSecurityPersister>
      transport_security_persister_;

  // These are only valid in between LazyInitialize() and their accessor being
  // called.
  mutable scoped_ptr<net::URLRequestContext> main_request_context_;
  mutable scoped_ptr<net::URLRequestContext> extensions_request_context_;
  // One URLRequestContext per isolated app for main and media requests.
  mutable URLRequestContextMap app_request_context_map_;
  mutable URLRequestContextMap isolated_media_request_context_map_;

  mutable scoped_ptr<ResourceContext> resource_context_;

  mutable scoped_refptr<CookieSettings> cookie_settings_;

  mutable scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  mutable scoped_ptr<ChromeHttpUserAgentSettings>
      chrome_http_user_agent_settings_;

#if defined(ENABLE_MANAGED_USERS)
  mutable scoped_refptr<const SupervisedUserURLFilter>
      supervised_user_url_filter_;
#endif

  mutable scoped_ptr<DevToolsNetworkController> network_controller_;

  // TODO(jhawkins): Remove once crbug.com/102004 is fixed.
  bool initialized_on_UI_thread_;

  const Profile::ProfileType profile_type_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

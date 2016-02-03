// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IO_DATA_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IO_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_member.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/net/net_types.h"
#include "net/cert/ct_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory.h"

class HostContentSettingsMap;
class IOSChromeHttpUserAgentSettings;
class IOSChromeNetworkDelegate;
class IOSChromeURLRequestContextGetter;

namespace content_settings {
class CookieSettings;
}

namespace data_reduction_proxy {
class DataReductionProxyIOData;
}

namespace ios {
class ChromeBrowserState;
enum class ChromeBrowserStateType;
}

namespace net {
class CertificateReportSender;
class CertVerifier;
class ChannelIDService;
class CookieStore;
class HttpServerProperties;
class HttpTransactionFactory;
class ProxyConfigService;
class ProxyService;
class ServerBoundCertService;
class SSLConfigService;
class TransportSecurityPersister;
class TransportSecurityState;
class URLRequestJobFactoryImpl;
}  // namespace net

// Conceptually speaking, the ChromeBrowserStateIOData represents data that
// lives on the IO thread that is owned by a ChromeBrowserState, such as, but
// not limited to, network objects like CookieMonster, HttpTransactionFactory,
// etc.
// ChromeBrowserState owns ChromeBrowserStateIOData, but will make sure to
// delete it on the IO thread.
class ChromeBrowserStateIOData {
 public:
  typedef std::vector<scoped_refptr<IOSChromeURLRequestContextGetter>>
      IOSChromeURLRequestContextGetterVector;

  virtual ~ChromeBrowserStateIOData();

  // Returns true if |scheme| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledProtocol(const std::string& scheme);

  // Utility to install additional WebUI handlers into the |job_factory|.
  // Ownership of the handlers is transfered from |protocol_handlers|
  // to the |job_factory|.
  static void InstallProtocolHandlers(
      net::URLRequestJobFactoryImpl* job_factory,
      ProtocolHandlerMap* protocol_handlers);

  // Initializes the ChromeBrowserStateIOData object and primes the
  // RequestContext generation. Must be called prior to any of the Get*()
  // methods other than GetResouceContext or GetMetricsEnabledStateOnIOThread.
  void Init(ProtocolHandlerMap* protocol_handlers) const;

  net::URLRequestContext* GetMainRequestContext() const;
  net::URLRequestContext* GetIsolatedAppRequestContext(
      net::URLRequestContext* main_context,
      const base::FilePath& partition_path) const;

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that browser state.
  content_settings::CookieSettings* GetCookieSettings() const;
  HostContentSettingsMap* GetHostContentSettingsMap() const;

  StringPrefMember* google_services_account_id() const {
    return &google_services_user_account_id_;
  }

  BooleanPrefMember* safe_browsing_enabled() const {
    return &safe_browsing_enabled_;
  }

  net::TransportSecurityState* transport_security_state() const {
    return transport_security_state_.get();
  }

  ios::ChromeBrowserStateType browser_state_type() const {
    return browser_state_type_;
  }

  bool IsOffTheRecord() const;

  // Initialize the member needed to track the metrics enabled state. This is
  // only to be called on the UI thread.
  void InitializeMetricsEnabledStateOnUIThread();

  // Returns whether or not metrics reporting is enabled in the browser instance
  // on which this browser state resides. This is safe for use from the IO
  // thread, and should only be called from there.
  bool GetMetricsEnabledStateOnIOThread() const;

  bool IsDataReductionProxyEnabled() const;

  data_reduction_proxy::DataReductionProxyIOData* data_reduction_proxy_io_data()
      const {
    return data_reduction_proxy_io_data_.get();
  }

 protected:
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
    ~AppRequestContext() override;

    scoped_refptr<net::CookieStore> cookie_store_;
    scoped_ptr<net::HttpTransactionFactory> http_factory_;
    scoped_ptr<net::URLRequestJobFactory> job_factory_;
  };

  // Created on the UI thread, read on the IO thread during
  // ChromeBrowserStateIOData lazy initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    base::FilePath path;
    IOSChromeIOThread* io_thread;
    scoped_refptr<content_settings::CookieSettings> cookie_settings;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_refptr<net::CookieMonsterDelegate> cookie_monster_delegate;

    // We need to initialize the ProxyConfigService from the UI thread
    // because on linux it relies on initializing things through gconf,
    // and needs to be on the main thread.
    scoped_ptr<net::ProxyConfigService> proxy_config_service;

    // The browser state this struct was populated from. It's passed as a void*
    // to ensure it's not accidently used on the IO thread.
    void* browser_state;
  };

  explicit ChromeBrowserStateIOData(
      ios::ChromeBrowserStateType browser_state_type);

  void InitializeOnUIThread(ios::ChromeBrowserState* browser_state);
  void ApplyProfileParamsToContext(net::URLRequestContext* context) const;

  scoped_ptr<net::URLRequestJobFactory> SetUpJobFactoryDefaults(
      scoped_ptr<net::URLRequestJobFactoryImpl> job_factory,
      URLRequestInterceptorScopedVector request_interceptors,
      net::NetworkDelegate* network_delegate) const;

  // Called when the ChromeBrowserState is destroyed. |context_getters| must
  // include all URLRequestContextGetters that refer to the
  // ChromeBrowserStateIOData's URLRequestContexts. Triggers destruction of the
  // ChromeBrowserStateIOData and shuts down |context_getters| safely on the IO
  // thread.
  // TODO(mmenke):  Passing all those URLRequestContextGetters around like this
  //     is really silly.  Can we do something cleaner?
  void ShutdownOnUIThread(
      scoped_ptr<IOSChromeURLRequestContextGetterVector> context_getters);

  // A ChannelIDService object is created by a derived class of
  // ChromeBrowserStateIOData, and the derived class calls this method to set
  // the channel_id_service_ member and transfers ownership to the base class.
  void set_channel_id_service(net::ChannelIDService* channel_id_service) const;

  void set_data_reduction_proxy_io_data(
      scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
          data_reduction_proxy_io_data) const;

  net::ProxyService* proxy_service() const { return proxy_service_.get(); }

  base::WeakPtr<net::HttpServerProperties> http_server_properties() const;

  void set_http_server_properties(
      scoped_ptr<net::HttpServerProperties> http_server_properties) const;

  net::URLRequestContext* main_request_context() const {
    return main_request_context_.get();
  }

  bool initialized() const { return initialized_; }

  scoped_ptr<net::HttpNetworkSession> CreateHttpNetworkSession(
      const ProfileParams& profile_params) const;

  // Creates main network transaction factory.
  scoped_ptr<net::HttpCache> CreateMainHttpFactory(
      net::HttpNetworkSession* session,
      scoped_ptr<net::HttpCache::BackendFactory> main_backend) const;

  // Creates network transaction factory.
  scoped_ptr<net::HttpCache> CreateHttpFactory(
      net::HttpNetworkSession* shared_session,
      scoped_ptr<net::HttpCache::BackendFactory> backend) const;

 private:
  typedef std::map<base::FilePath, net::URLRequestContext*>
      URLRequestContextMap;

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does the actual initialization of the ChromeBrowserStateIOData subtype.
  // Subtypes should use the static helper functions above to implement this.
  virtual void InitializeInternal(
      scoped_ptr<IOSChromeNetworkDelegate> chrome_network_delegate,
      ProfileParams* profile_params,
      ProtocolHandlerMap* protocol_handlers) const = 0;

  // Does an on-demand initialization of a RequestContext for the given
  // isolated app.
  virtual net::URLRequestContext* InitializeAppRequestContext(
      net::URLRequestContext* main_context) const = 0;

  // These functions are used to transfer ownership of the lazily initialized
  // context from ChromeBrowserStateIOData to the URLRequestContextGetter.
  virtual net::URLRequestContext* AcquireIsolatedAppRequestContext(
      net::URLRequestContext* main_context) const = 0;

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

  // Data from the UI thread from the ChromeBrowserState, used to initialize
  // ChromeBrowserStateIOData. Deleted after lazy initialization.
  mutable scoped_ptr<ProfileParams> profile_params_;

  mutable StringPrefMember google_services_user_account_id_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember enable_referrers_;
  mutable BooleanPrefMember enable_do_not_track_;
  mutable BooleanPrefMember safe_browsing_enabled_;
  mutable BooleanPrefMember sync_disabled_;
  mutable BooleanPrefMember signin_allowed_;

  BooleanPrefMember enable_metrics_;

  // Pointed to by URLRequestContext.
  mutable scoped_ptr<net::ChannelIDService> channel_id_service_;

  mutable scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data_;

  mutable scoped_ptr<net::ProxyService> proxy_service_;
  mutable scoped_ptr<net::TransportSecurityState> transport_security_state_;
  mutable scoped_ptr<net::CTVerifier> cert_transparency_verifier_;
  mutable scoped_ptr<net::HttpServerProperties> http_server_properties_;
  mutable scoped_ptr<net::TransportSecurityPersister>
      transport_security_persister_;
  mutable scoped_ptr<net::CertificateReportSender> certificate_report_sender_;

  // These are only valid in between LazyInitialize() and their accessor being
  // called.
  mutable scoped_ptr<net::URLRequestContext> main_request_context_;
  // One URLRequestContext per isolated app for main and media requests.
  mutable URLRequestContextMap app_request_context_map_;

  mutable scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  mutable scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  mutable scoped_ptr<IOSChromeHttpUserAgentSettings>
      chrome_http_user_agent_settings_;

  // TODO(jhawkins): Remove once crbug.com/102004 is fixed.
  bool initialized_on_UI_thread_;

  const ios::ChromeBrowserStateType browser_state_type_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserStateIOData);
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IO_DATA_H_

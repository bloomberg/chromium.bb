// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IO_THREAD_H_
#define CHROME_BROWSER_IO_THREAD_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_network_session.h"
#include "net/socket/next_proto.h"

class ChromeNetLog;
class CommandLine;
class PrefProxyConfigTrackerImpl;
class PrefService;
class PrefRegistrySimple;
class SystemURLRequestContextGetter;

namespace chrome_browser_net {
class DnsProbeService;
class HttpPipeliningCompatibilityClient;
class LoadTimeStats;
}

namespace extensions {
class EventRouterForwarder;
}

namespace net {
class CertVerifier;
class CookieStore;
class FtpTransactionFactory;
class HostMappingRules;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpServerProperties;
class HttpTransactionFactory;
class HttpUserAgentSettings;
class NetworkDelegate;
class ServerBoundCertService;
class ProxyConfigService;
class ProxyService;
class SdchManager;
class SSLConfigService;
class TransportSecurityState;
class URLRequestContext;
class URLRequestContextGetter;
class URLRequestThrottlerManager;
class URLSecurityManager;
}  // namespace net

namespace policy {
class PolicyService;
}  // namespace policy

// Contains state associated with, initialized and cleaned up on, and
// primarily used on, the IO thread.
//
// If you are looking to interact with the IO thread (e.g. post tasks
// to it or check if it is the current thread), see
// content::BrowserThread.
class IOThread : public content::BrowserThreadDelegate {
 public:
  struct Globals {
    template <typename T>
    class Optional {
     public:
      Optional() : set_(false) {}

      void set(T value) {
        set_ = true;
        value_ = value;
      }
      void CopyToIfSet(T* value) {
        if (set_) {
          *value = value_;
        }
      }

     private:
      bool set_;
      T value_;
    };

    class SystemRequestContextLeakChecker {
     public:
      explicit SystemRequestContextLeakChecker(Globals* globals);
      ~SystemRequestContextLeakChecker();

     private:
      Globals* const globals_;
    };

    Globals();
    ~Globals();

    // The "system" NetworkDelegate, used for Profile-agnostic network events.
    scoped_ptr<net::NetworkDelegate> system_network_delegate;
    scoped_ptr<net::HostResolver> host_resolver;
    scoped_ptr<net::CertVerifier> cert_verifier;
    // The ServerBoundCertService must outlive the HttpTransactionFactory.
    scoped_ptr<net::ServerBoundCertService> system_server_bound_cert_service;
    // This TransportSecurityState doesn't load or save any state. It's only
    // used to enforce pinning for system requests and will only use built-in
    // pins.
    scoped_ptr<net::TransportSecurityState> transport_security_state;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory;
    scoped_ptr<net::HttpServerProperties> http_server_properties;
    scoped_ptr<net::ProxyService> proxy_script_fetcher_proxy_service;
    scoped_ptr<net::HttpTransactionFactory>
        proxy_script_fetcher_http_transaction_factory;
    scoped_ptr<net::FtpTransactionFactory>
        proxy_script_fetcher_ftp_transaction_factory;
    scoped_ptr<net::URLRequestThrottlerManager> throttler_manager;
    scoped_ptr<net::URLSecurityManager> url_security_manager;
    // TODO(willchan): Remove proxy script fetcher context since it's not
    // necessary now that I got rid of refcounting URLRequestContexts.
    //
    // The first URLRequestContext is |system_url_request_context|. We introduce
    // |proxy_script_fetcher_context| for the second context. It has a direct
    // ProxyService, since we always directly connect to fetch the PAC script.
    scoped_ptr<net::URLRequestContext> proxy_script_fetcher_context;
    scoped_ptr<net::ProxyService> system_proxy_service;
    scoped_ptr<net::HttpTransactionFactory> system_http_transaction_factory;
    scoped_ptr<net::FtpTransactionFactory> system_ftp_transaction_factory;
    scoped_ptr<net::URLRequestContext> system_request_context;
    SystemRequestContextLeakChecker system_request_context_leak_checker;
    // |system_cookie_store| and |system_server_bound_cert_service| are shared
    // between |proxy_script_fetcher_context| and |system_request_context|.
    scoped_refptr<net::CookieStore> system_cookie_store;
    scoped_refptr<extensions::EventRouterForwarder>
        extension_event_router_forwarder;
    scoped_ptr<chrome_browser_net::HttpPipeliningCompatibilityClient>
        http_pipelining_compatibility_client;
    scoped_ptr<chrome_browser_net::LoadTimeStats> load_time_stats;
    scoped_ptr<net::HostMappingRules> host_mapping_rules;
    scoped_ptr<net::HttpUserAgentSettings> http_user_agent_settings;
    bool ignore_certificate_errors;
    bool http_pipelining_enabled;
    uint16 testing_fixed_http_port;
    uint16 testing_fixed_https_port;
    Optional<size_t> max_spdy_sessions_per_domain;
    Optional<size_t> initial_max_spdy_concurrent_streams;
    Optional<size_t> max_spdy_concurrent_streams_limit;
    Optional<bool> force_spdy_single_domain;
    Optional<bool> enable_spdy_ip_pooling;
    Optional<bool> enable_spdy_credential_frames;
    Optional<bool> enable_spdy_compression;
    Optional<bool> enable_spdy_ping_based_connection_checking;
    Optional<net::NextProto> spdy_default_protocol;
    Optional<bool> enable_quic;
    Optional<uint16> origin_port_to_force_quic_on;
    Optional<bool> use_spdy_over_quic;
    bool enable_user_alternate_protocol_ports;
    // NetErrorTabHelper uses |dns_probe_service| to send DNS probes when a
    // main frame load fails with a DNS error in order to provide more useful
    // information to the renderer so it can show a more specific error page.
    scoped_ptr<chrome_browser_net::DnsProbeService> dns_probe_service;
  };

  // |net_log| must either outlive the IOThread or be NULL.
  IOThread(PrefService* local_state,
           policy::PolicyService* policy_service,
           ChromeNetLog* net_log,
           extensions::EventRouterForwarder* extension_event_router_forwarder);

  virtual ~IOThread();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Can only be called on the IO thread.
  Globals* globals();

  ChromeNetLog* net_log();

  // Handles changing to On The Record mode, discarding confidential data.
  void ChangedToOnTheRecord();

  // Returns a getter for the URLRequestContext.  Only called on the UI thread.
  net::URLRequestContextGetter* system_url_request_context_getter();

  // Clears the host cache.  Intended to be used to prevent exposing recently
  // visited sites on about:net-internals/#dns and about:dns pages.  Must be
  // called on the IO thread.
  void ClearHostCache();

  void InitializeNetworkSessionParams(net::HttpNetworkSession::Params* params);

 private:
  // Provide SystemURLRequestContextGetter with access to
  // InitSystemRequestContext().
  friend class SystemURLRequestContextGetter;

  // BrowserThreadDelegate implementation, runs on the IO thread.
  // This handles initialization and destruction of state that must
  // live on the IO thread.
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  void InitializeNetworkOptions(const CommandLine& parsed_command_line);

  // Enable the SPDY protocol.  If this function is not called, SPDY/3
  // will be enabled.
  //   "off"                      : Disables SPDY support entirely.
  //   "ssl"                      : Forces SPDY for all HTTPS requests.
  //   "no-ssl"                   : Forces SPDY for all HTTP requests.
  //   "no-ping"                  : Disables SPDY ping connection testing.
  //   "exclude=<host>"           : Disables SPDY support for the host <host>.
  //   "no-compress"              : Disables SPDY header compression.
  //   "no-alt-protocols          : Disables alternate protocol support.
  //   "force-alt-protocols       : Forces an alternate protocol of SPDY/2
  //                                on port 443.
  //   "single-domain"            : Forces all spdy traffic to a single domain.
  //   "init-max-streams=<limit>" : Specifies the maximum number of concurrent
  //                                streams for a SPDY session, unless the
  //                                specifies a different value via SETTINGS.
  void EnableSpdy(const std::string& mode);

  // Global state must be initialized on the IO thread, then this
  // method must be invoked on the UI thread.
  void InitSystemRequestContext();

  // Lazy initialization of system request context for
  // SystemURLRequestContextGetter. To be called on IO thread only
  // after global state has been initialized on the IO thread, and
  // SystemRequestContext state has been initialized on the UI thread.
  void InitSystemRequestContextOnIOThread();

  net::HttpAuthHandlerFactory* CreateDefaultAuthHandlerFactory(
      net::HostResolver* resolver);

  // Returns an SSLConfigService instance.
  net::SSLConfigService* GetSSLConfigService();

  void ChangedToOnTheRecordOnIOThread();

  void UpdateDnsClientEnabled();

  // The NetLog is owned by the browser process, to allow logging from other
  // threads during shutdown, but is used most frequently on the IOThread.
  ChromeNetLog* net_log_;

  // The extensions::EventRouterForwarder allows for sending events to
  // extensions from the IOThread.
  extensions::EventRouterForwarder* extension_event_router_forwarder_;

  // These member variables are basically global, but their lifetimes are tied
  // to the IOThread.  IOThread owns them all, despite not using scoped_ptr.
  // This is because the destructor of IOThread runs on the wrong thread.  All
  // member variables should be deleted in CleanUp().

  // These member variables are initialized in Init() and do not change for the
  // lifetime of the IO thread.

  Globals* globals_;

  // Observer that logs network changes to the ChromeNetLog.
  class LoggingNetworkChangeObserver;
  scoped_ptr<LoggingNetworkChangeObserver> network_change_observer_;

  BooleanPrefMember system_enable_referrers_;

  BooleanPrefMember dns_client_enabled_;

  // Store HTTP Auth-related policies in this thread.
  std::string auth_schemes_;
  bool negotiate_disable_cname_lookup_;
  bool negotiate_enable_port_;
  std::string auth_server_whitelist_;
  std::string auth_delegate_whitelist_;
  std::string gssapi_library_name_;
  std::string spdyproxy_origin_;

  // This is an instance of the default SSLConfigServiceManager for the current
  // platform and it gets SSL preferences from local_state object.
  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  // These member variables are initialized by a task posted to the IO thread,
  // which gets posted by calling certain member functions of IOThread.
  scoped_ptr<net::ProxyConfigService> system_proxy_config_service_;

  scoped_ptr<PrefProxyConfigTrackerImpl> pref_proxy_config_tracker_;

  scoped_refptr<net::URLRequestContextGetter>
      system_url_request_context_getter_;

  net::SdchManager* sdch_manager_;

  // True if SPDY is disabled by policy.
  bool is_spdy_disabled_by_policy_;

  base::WeakPtrFactory<IOThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThread);
};

#endif  // CHROME_BROWSER_IO_THREAD_H_

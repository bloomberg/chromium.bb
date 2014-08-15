// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IO_THREAD_H_
#define CHROME_BROWSER_IO_THREAD_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/time/time.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_network_session.h"
#include "net/socket/next_proto.h"

class ChromeNetLog;
class PrefProxyConfigTracker;
class PrefService;
class PrefRegistrySimple;
class SystemURLRequestContextGetter;

namespace base {
class CommandLine;
}

namespace chrome_browser_net {
class DnsProbeService;
}

#if defined(SPDY_PROXY_AUTH_ORIGIN)
namespace data_reduction_proxy {
class DataReductionProxyAuthRequestHandler;
class DataReductionProxyParams;
}
#endif  // defined(SPDY_PROXY_AUTH_ORIGIN)

namespace extensions {
class EventRouterForwarder;
}

namespace net {
class CertVerifier;
class ChannelIDService;
class CookieStore;
class CTVerifier;
class FtpTransactionFactory;
class HostMappingRules;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpServerProperties;
class HttpTransactionFactory;
class HttpUserAgentSettings;
class NetworkDelegate;
class ProxyConfigService;
class ProxyService;
class SSLConfigService;
class TransportSecurityState;
class URLRequestContext;
class URLRequestContextGetter;
class URLRequestJobFactory;
class URLRequestThrottlerManager;
class URLSecurityManager;
}  // namespace net

namespace policy {
class PolicyService;
}  // namespace policy

namespace test {
class IOThreadPeer;
}  // namespace test

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
      void CopyToIfSet(T* value) const {
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
    // The ChannelIDService must outlive the HttpTransactionFactory.
    scoped_ptr<net::ChannelIDService> system_channel_id_service;
    // This TransportSecurityState doesn't load or save any state. It's only
    // used to enforce pinning for system requests and will only use built-in
    // pins.
    scoped_ptr<net::TransportSecurityState> transport_security_state;
    scoped_ptr<net::CTVerifier> cert_transparency_verifier;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory;
    scoped_ptr<net::HttpServerProperties> http_server_properties;
    scoped_ptr<net::ProxyService> proxy_script_fetcher_proxy_service;
    scoped_ptr<net::HttpTransactionFactory>
        proxy_script_fetcher_http_transaction_factory;
    scoped_ptr<net::FtpTransactionFactory>
        proxy_script_fetcher_ftp_transaction_factory;
    scoped_ptr<net::URLRequestJobFactory>
        proxy_script_fetcher_url_request_job_factory;
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
    scoped_ptr<net::URLRequestJobFactory> system_url_request_job_factory;
    scoped_ptr<net::URLRequestContext> system_request_context;
    SystemRequestContextLeakChecker system_request_context_leak_checker;
    // |system_cookie_store| and |system_channel_id_service| are shared
    // between |proxy_script_fetcher_context| and |system_request_context|.
    scoped_refptr<net::CookieStore> system_cookie_store;
#if defined(ENABLE_EXTENSIONS)
    scoped_refptr<extensions::EventRouterForwarder>
        extension_event_router_forwarder;
#endif
    scoped_ptr<net::HostMappingRules> host_mapping_rules;
    scoped_ptr<net::HttpUserAgentSettings> http_user_agent_settings;
    bool enable_ssl_connect_job_waiting;
    bool ignore_certificate_errors;
    uint16 testing_fixed_http_port;
    uint16 testing_fixed_https_port;

    Optional<size_t> initial_max_spdy_concurrent_streams;
    Optional<bool> force_spdy_single_domain;
    Optional<bool> enable_spdy_compression;
    Optional<bool> enable_spdy_ping_based_connection_checking;
    Optional<net::NextProto> spdy_default_protocol;
    net::NextProtoVector next_protos;
    Optional<string> trusted_spdy_proxy;
    Optional<bool> force_spdy_over_ssl;
    Optional<bool> force_spdy_always;
    std::set<net::HostPortPair> forced_spdy_exclusions;
    Optional<bool> use_alternate_protocols;
    Optional<double> alternate_protocol_probability_threshold;
    Optional<bool> enable_websocket_over_spdy;

    Optional<bool> enable_quic;
    Optional<bool> enable_quic_time_based_loss_detection;
    Optional<bool> enable_quic_port_selection;
    Optional<size_t> quic_max_packet_length;
    net::QuicTagVector quic_connection_options;
    Optional<std::string> quic_user_agent_id;
    Optional<net::QuicVersionVector> quic_supported_versions;
    Optional<net::HostPortPair> origin_to_force_quic_on;
    bool enable_user_alternate_protocol_ports;
    // NetErrorTabHelper uses |dns_probe_service| to send DNS probes when a
    // main frame load fails with a DNS error in order to provide more useful
    // information to the renderer so it can show a more specific error page.
    scoped_ptr<chrome_browser_net::DnsProbeService> dns_probe_service;
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  scoped_ptr<data_reduction_proxy::DataReductionProxyParams>
      data_reduction_proxy_params;
  scoped_ptr<data_reduction_proxy::DataReductionProxyAuthRequestHandler>
      data_reduction_proxy_auth_request_handler;
#endif
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

  // Allows overriding Globals in tests where IOThread::Init() and
  // IOThread::CleanUp() are not called.  This allows for injecting mocks into
  // IOThread global objects.
  void SetGlobalsForTesting(Globals* globals);

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

  base::TimeTicks creation_time() const;

 private:
  // Map from name to value for all parameters associate with a field trial.
  typedef std::map<std::string, std::string> VariationParameters;

  // Provide SystemURLRequestContextGetter with access to
  // InitSystemRequestContext().
  friend class SystemURLRequestContextGetter;

  friend class test::IOThreadPeer;

  // BrowserThreadDelegate implementation, runs on the IO thread.
  // This handles initialization and destruction of state that must
  // live on the IO thread.
  virtual void Init() OVERRIDE;
  virtual void InitAsync() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  // Initializes |params| based on the settings in |globals|.
  static void InitializeNetworkSessionParamsFromGlobals(
      const Globals& globals,
      net::HttpNetworkSession::Params* params);

  void InitializeNetworkOptions(const base::CommandLine& parsed_command_line);

  // Enable SPDY with the given mode, which may contain the following:
  //
  //   "off"                      : Disables SPDY support entirely.
  //   "ssl"                      : Forces SPDY for all HTTPS requests.
  //   "no-ssl"                   : Forces SPDY for all HTTP requests.
  //   "no-ping"                  : Disables SPDY ping connection testing.
  //   "exclude=<host>"           : Disables SPDY support for the host <host>.
  //   "no-compress"              : Disables SPDY header compression.
  //   "no-alt-protocols          : Disables alternate protocol support.
  //   "force-alt-protocols       : Forces an alternate protocol of SPDY/3
  //                                on port 443.
  //   "single-domain"            : Forces all spdy traffic to a single domain.
  //   "init-max-streams=<limit>" : Specifies the maximum number of concurrent
  //                                streams for a SPDY session, unless the
  //                                specifies a different value via SETTINGS.
  void EnableSpdy(const std::string& mode);

  // Configures available SPDY protocol versions from the given trial.
  // Used only if no command-line configuration was present.
  static void ConfigureSpdyFromTrial(const std::string& spdy_trial_group,
                                     Globals* globals);

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

  // Configures QUIC options based on the flags in |command_line| as
  // well as the QUIC field trial group.
  void ConfigureQuic(const base::CommandLine& command_line);

  extensions::EventRouterForwarder* extension_event_router_forwarder() {
#if defined(ENABLE_EXTENSIONS)
    return extension_event_router_forwarder_;
#else
    return NULL;
#endif
  }
  // Configures QUIC options in |globals| based on the flags in |command_line|
  // as well as the QUIC field trial group and parameters.
  static void ConfigureQuicGlobals(
      const base::CommandLine& command_line,
      base::StringPiece quic_trial_group,
      const VariationParameters& quic_trial_params,
      Globals* globals);

  // Returns true if QUIC should be enabled, either as a result
  // of a field trial or a command line flag.
  static bool ShouldEnableQuic(
      const base::CommandLine& command_line,
      base::StringPiece quic_trial_group);

  // Returns true if the selection of the ephemeral port in bind() should be
  // performed by Chromium, and false if the OS should select the port.  The OS
  // option is used to prevent Windows from posting a security security warning
  // dialog.
  static bool ShouldEnableQuicPortSelection(
      const base::CommandLine& command_line);

  // Returns true if QUIC packet pacing should be negotiated during the
  // QUIC handshake.
  static bool ShouldEnableQuicPacing(
      const base::CommandLine& command_line,
      base::StringPiece quic_trial_group,
      const VariationParameters& quic_trial_params);

  // Returns true if QUIC time-base loss detection should be negotiated during
  // the QUIC handshake.
  static bool ShouldEnableQuicTimeBasedLossDetection(
      const base::CommandLine& command_line,
      base::StringPiece quic_trial_group,
      const VariationParameters& quic_trial_params);

  // Returns the maximum length for QUIC packets, based on any flags in
  // |command_line| or the field trial.  Returns 0 if there is an error
  // parsing any of the options, or if the default value should be used.
  static size_t GetQuicMaxPacketLength(
      const base::CommandLine& command_line,
      base::StringPiece quic_trial_group,
      const VariationParameters& quic_trial_params);

  // Returns the QUIC versions specified by any flags in |command_line|
  // or |quic_trial_params|.
  static net::QuicVersion GetQuicVersion(
      const base::CommandLine& command_line,
      const VariationParameters& quic_trial_params);

  // Returns the QUIC version specified by |quic_version| or
  // QUIC_VERSION_UNSUPPORTED if |quic_version| is invalid.
  static net::QuicVersion ParseQuicVersion(const std::string& quic_version);

  // Returns the QUIC connection options specified by any flags in
  // |command_line| or |quic_trial_params|.
  static net::QuicTagVector GetQuicConnectionOptions(
      const base::CommandLine& command_line,
      const VariationParameters& quic_trial_params);

  // Returns the list of QUIC tags represented by the comma separated
  // string in |connection_options|.
  static net::QuicTagVector ParseQuicConnectionOptions(
      const std::string& connection_options);

  // Returns the alternate protocol probability threshold specified by
  // any flags in |command_line| or |quic_trial_params|.
  static double GetAlternateProtocolProbabilityThreshold(
      const base::CommandLine& command_line,
      const VariationParameters& quic_trial_params);

  // The NetLog is owned by the browser process, to allow logging from other
  // threads during shutdown, but is used most frequently on the IOThread.
  ChromeNetLog* net_log_;

#if defined(ENABLE_EXTENSIONS)
  // The extensions::EventRouterForwarder allows for sending events to
  // extensions from the IOThread.
  extensions::EventRouterForwarder* extension_event_router_forwarder_;
#endif

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

  BooleanPrefMember quick_check_enabled_;

  // Store HTTP Auth-related policies in this thread.
  std::string auth_schemes_;
  bool negotiate_disable_cname_lookup_;
  bool negotiate_enable_port_;
  std::string auth_server_whitelist_;
  std::string auth_delegate_whitelist_;
  std::string gssapi_library_name_;

  // This is an instance of the default SSLConfigServiceManager for the current
  // platform and it gets SSL preferences from local_state object.
  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  // These member variables are initialized by a task posted to the IO thread,
  // which gets posted by calling certain member functions of IOThread.
  scoped_ptr<net::ProxyConfigService> system_proxy_config_service_;

  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_refptr<net::URLRequestContextGetter>
      system_url_request_context_getter_;

  // True if SPDY is disabled by policy.
  bool is_spdy_disabled_by_policy_;

  base::WeakPtrFactory<IOThread> weak_factory_;

  const base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(IOThread);
};

#endif  // CHROME_BROWSER_IO_THREAD_H_

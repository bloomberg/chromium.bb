// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_IO_THREAD_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_IO_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/ssl_config/ssl_config_service_manager.h"
#include "ios/web/public/web_thread_delegate.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_network_session.h"

class PrefProxyConfigTracker;
class PrefService;
class SystemURLRequestContextGetter;

namespace base {
class CommandLine;
}  // namespace base

namespace net {
class CTPolicyEnforcer;
class CertVerifier;
class ChannelIDService;
class CookieStore;
class CTVerifier;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpAuthPreferences;
class HttpNetworkSession;
class HttpServerProperties;
class HttpTransactionFactory;
class HttpUserAgentSettings;
class NetworkDelegate;
class NetworkQualityEstimator;
class ProxyConfigService;
class ProxyService;
class SSLConfigService;
class TransportSecurityState;
class URLRequestBackoffManager;
class URLRequestContext;
class URLRequestContextGetter;
class URLRequestJobFactory;
}  // namespace net

namespace net_log {
class ChromeNetLog;
}  // namespace net_log

// Contains state associated with, initialized and cleaned up on, and
// primarily used on, the IO thread.
//
// If you are looking to interact with the IO thread (e.g. post tasks
// to it or check if it is the current thread), see web::WebThread.
class IOSChromeIOThread : public web::WebThreadDelegate {
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

    // The "system" NetworkDelegate, used for BrowserState-agnostic network
    // events.
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
    scoped_ptr<net::CTPolicyEnforcer> ct_policy_enforcer;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_ptr<net::HttpAuthPreferences> http_auth_preferences;
    scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory;
    scoped_ptr<net::HttpServerProperties> http_server_properties;
    scoped_ptr<net::URLRequestBackoffManager> url_request_backoff_manager;
    scoped_ptr<net::ProxyService> system_proxy_service;
    scoped_ptr<net::HttpNetworkSession> system_http_network_session;
    scoped_ptr<net::HttpTransactionFactory> system_http_transaction_factory;
    scoped_ptr<net::URLRequestJobFactory> system_url_request_job_factory;
    scoped_ptr<net::URLRequestContext> system_request_context;
    SystemRequestContextLeakChecker system_request_context_leak_checker;
    scoped_refptr<net::CookieStore> system_cookie_store;
    scoped_ptr<net::HttpUserAgentSettings> http_user_agent_settings;
    scoped_ptr<net::NetworkQualityEstimator> network_quality_estimator;
    uint16_t testing_fixed_http_port;
    uint16_t testing_fixed_https_port;
    Optional<bool> enable_tcp_fast_open_for_ssl;

    Optional<size_t> initial_max_spdy_concurrent_streams;
    Optional<bool> enable_spdy_compression;
    Optional<bool> enable_spdy_ping_based_connection_checking;
    Optional<bool> enable_spdy31;
    Optional<bool> enable_http2;
    std::set<net::HostPortPair> forced_spdy_exclusions;
    Optional<bool> parse_alternative_services;
    Optional<bool> enable_alternative_service_with_different_host;
    Optional<double> alternative_service_probability_threshold;

    Optional<bool> enable_npn;

    Optional<bool> enable_quic;
    Optional<bool> enable_quic_for_proxies;
    Optional<bool> quic_always_require_handshake_confirmation;
    Optional<bool> quic_disable_connection_pooling;
    Optional<float> quic_load_server_info_timeout_srtt_multiplier;
    Optional<bool> quic_enable_connection_racing;
    Optional<bool> quic_enable_non_blocking_io;
    Optional<bool> quic_disable_disk_cache;
    Optional<bool> quic_prefer_aes;
    Optional<int> quic_max_number_of_lossy_connections;
    Optional<float> quic_packet_loss_threshold;
    Optional<int> quic_socket_receive_buffer_size;
    Optional<bool> quic_delay_tcp_race;
    Optional<size_t> quic_max_packet_length;
    net::QuicTagVector quic_connection_options;
    Optional<std::string> quic_user_agent_id;
    Optional<net::QuicVersionVector> quic_supported_versions;
    Optional<bool> quic_close_sessions_on_ip_change;
  };

  // |net_log| must either outlive the IOSChromeIOThread or be NULL.
  IOSChromeIOThread(PrefService* local_state, net_log::ChromeNetLog* net_log);

  ~IOSChromeIOThread() override;

  // Can only be called on the IO thread.
  Globals* globals();

  // Allows overriding Globals in tests where IOSChromeIOThread::Init() and
  // IOSChromeIOThread::CleanUp() are not called.  This allows for injecting
  // mocks into IOSChromeIOThread global objects.
  void SetGlobalsForTesting(Globals* globals);

  net_log::ChromeNetLog* net_log();

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

  // Returns true if QUIC should be enabled for data reduction proxy as a result
  // of a field trial.
  static bool ShouldEnableQuicForDataReductionProxy();

 private:
  // Map from name to value for all parameters associate with a field trial.
  typedef std::map<std::string, std::string> VariationParameters;

  // Provide SystemURLRequestContextGetter with access to
  // InitSystemRequestContext().
  friend class SystemURLRequestContextGetter;

  // WebThreadDelegate implementation, runs on the IO thread.
  // This handles initialization and destruction of state that must
  // live on the IO thread.
  void Init() override;
  void CleanUp() override;

  // Initializes |params| based on the settings in |globals|.
  static void InitializeNetworkSessionParamsFromGlobals(
      const Globals& globals,
      net::HttpNetworkSession::Params* params);

  void InitializeNetworkOptions();

  // Sets up SSL TCP FastOpen if enabled via field trials.
  void ConfigureSSLTCPFastOpen();

  // Configures available SPDY protocol versions in |globals| based on the SPDY
  // field trial group and parameters.
  // Must be called after ConfigureQuicGlobals.
  static void ConfigureSpdyGlobals(base::StringPiece quic_trial_group,
                                   const VariationParameters& quic_trial_params,
                                   Globals* globals);

  // Configures Alternative Services in |globals| based on the field trial
  // group.
  static void ConfigureAltSvcGlobals(base::StringPiece altsvc_trial_group,
                                     IOSChromeIOThread::Globals* globals);

  // Configures NPN in |globals| based on the field trial group.
  static void ConfigureNPNGlobals(base::StringPiece npn_trial_group,
                                  Globals* globals);

  // Global state must be initialized on the IO thread, then this
  // method must be invoked on the UI thread.
  void InitSystemRequestContext();

  // Lazy initialization of system request context for
  // SystemURLRequestContextGetter. To be called on IO thread only
  // after global state has been initialized on the IO thread, and
  // SystemRequestContext state has been initialized on the UI thread.
  void InitSystemRequestContextOnIOThread();

  void CreateDefaultAuthHandlerFactory();

  // Returns an SSLConfigService instance.
  net::SSLConfigService* GetSSLConfigService();

  void ChangedToOnTheRecordOnIOThread();

  // Configures QUIC options based on the QUIC field trial group.
  void ConfigureQuic();

  // Configures QUIC options in |globals| based on the flags in |command_line|
  // as well as the QUIC field trial group and parameters.
  // Must be called before ConfigureSpdyGlobals.
  static void ConfigureQuicGlobals(base::StringPiece quic_trial_group,
                                   const VariationParameters& quic_trial_params,
                                   Globals* globals);

  // Returns true if QUIC should be enabled as a result of a field trial.
  static bool ShouldEnableQuic(base::StringPiece quic_trial_group);

  // Returns true if QUIC should be enabled for proxies as a result of a
  // field trial.
  static bool ShouldEnableQuicForProxies(base::StringPiece quic_trial_group);

  // Returns true if QUIC should always require handshake confirmation during
  // the QUIC handshake.
  static bool ShouldQuicAlwaysRequireHandshakeConfirmation(
      const VariationParameters& quic_trial_params);

  // Returns true if QUIC should disable connection pooling.
  static bool ShouldQuicDisableConnectionPooling(
      const VariationParameters& quic_trial_params);

  // Returns the ratio of time to load QUIC sever information from disk cache to
  // 'smoothed RTT' based on field trial. Returns 0 if there is an error parsing
  // the field trial params, or if the default value should be used.
  static float GetQuicLoadServerInfoTimeoutSrttMultiplier(
      const VariationParameters& quic_trial_params);

  // Returns true if QUIC's connection racing should be enabled.
  static bool ShouldQuicEnableConnectionRacing(
      const VariationParameters& quic_trial_params);

  // Returns true if QUIC's should use non-blocking IO.
  static bool ShouldQuicEnableNonBlockingIO(
      const VariationParameters& quic_trial_params);

  // Returns true if QUIC shouldn't load QUIC server information from the disk
  // cache.
  static bool ShouldQuicDisableDiskCache(
      const VariationParameters& quic_trial_params);

  // Returns true if QUIC should prefer AES-GCN even without hardware support.
  static bool ShouldQuicPreferAes(const VariationParameters& quic_trial_params);

  // Returns true if QUIC should enable alternative services for different host.
  static bool ShouldQuicEnableAlternativeServicesForDifferentHost(
      const VariationParameters& quic_trial_params);

  // Returns the maximum number of QUIC connections with high packet loss in a
  // row after which QUIC should be disabled.  Returns 0 if the default value
  // should be used.
  static int GetQuicMaxNumberOfLossyConnections(
      const VariationParameters& quic_trial_params);

  // Returns the packet loss rate in fraction after which a QUIC connection is
  // closed and is considered as a lossy connection. Returns 0 if the default
  // value should be used.
  static float GetQuicPacketLossThreshold(
      const VariationParameters& quic_trial_params);

  // Returns the size of the QUIC receive buffer to use, or 0 if
  // the default should be used.
  static int GetQuicSocketReceiveBufferSize(
      const VariationParameters& quic_trial_params);

  // Returns true if QUIC should delay TCP connection when QUIC works.
  static bool ShouldQuicDelayTcpRace(
      const VariationParameters& quic_trial_params);

  // Returns true if QUIC should close sessions when any of the client's
  // IP addresses change.
  static bool ShouldQuicCloseSessionsOnIpChange(
      const VariationParameters& quic_trial_params);

  // Returns the maximum length for QUIC packets, based on any flags in the
  // field trial.  Returns 0 if there is an error parsing any of the options,
  // or if the default value should be used.
  static size_t GetQuicMaxPacketLength(
      const VariationParameters& quic_trial_params);

  // Returns the QUIC versions specified by any flags in |quic_trial_params|.
  static net::QuicVersion GetQuicVersion(
      const VariationParameters& quic_trial_params);

  // Returns the QUIC version specified by |quic_version| or
  // QUIC_VERSION_UNSUPPORTED if |quic_version| is invalid.
  static net::QuicVersion ParseQuicVersion(const std::string& quic_version);

  // Returns the QUIC connection options specified by any flags in
  // |quic_trial_params|.
  static net::QuicTagVector GetQuicConnectionOptions(
      const VariationParameters& quic_trial_params);

  // Returns the alternative service probability threshold specified by
  // any flags in |quic_trial_params|.
  static double GetAlternativeProtocolProbabilityThreshold(
      const VariationParameters& quic_trial_params);

  static net::URLRequestContext* ConstructSystemRequestContext(
      Globals* globals,
      net::NetLog* net_log);

  // The NetLog is owned by the application context, to allow logging from other
  // threads during shutdown, but is used most frequently on the IO thread.
  net_log::ChromeNetLog* net_log_;

  // These member variables are basically global, but their lifetimes are tied
  // to the IOSChromeIOThread.  IOSChromeIOThread owns them all, despite not
  // using scoped_ptr. This is because the destructor of IOSChromeIOThread runs
  // on the wrong thread.  All member variables should be deleted in CleanUp().

  // These member variables are initialized in Init() and do not change for the
  // lifetime of the IO thread.

  Globals* globals_;

  // Observer that logs network changes to the ChromeNetLog.
  class LoggingNetworkChangeObserver;
  scoped_ptr<LoggingNetworkChangeObserver> network_change_observer_;

  // This is an instance of the default SSLConfigServiceManager for the current
  // platform and it gets SSL preferences from local_state object.
  scoped_ptr<ssl_config::SSLConfigServiceManager> ssl_config_service_manager_;

  // These member variables are initialized by a task posted to the IO thread,
  // which gets posted by calling certain member functions of IOSChromeIOThread.
  scoped_ptr<net::ProxyConfigService> system_proxy_config_service_;

  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_refptr<net::URLRequestContextGetter>
      system_url_request_context_getter_;

  const base::TimeTicks creation_time_;

  base::WeakPtrFactory<IOSChromeIOThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeIOThread);
};

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_IO_THREAD_H_

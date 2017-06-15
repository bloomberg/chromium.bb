// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IO_THREAD_H_
#define CHROME_BROWSER_IO_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/common/features.h"
#include "components/metrics/data_use_tracker.h"
#include "components/prefs/pref_member.h"
#include "components/ssl_config/ssl_config_service_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "extensions/features/features.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_network_session.h"
#include "net/nqe/network_quality_estimator.h"

class PrefProxyConfigTracker;
class PrefService;
class PrefRegistrySimple;

#if defined(OS_ANDROID)
namespace chrome {
namespace android {
class ExternalDataUseObserver;
}
}
#endif  // defined(OS_ANDROID)

namespace base {
class CommandLine;
}

namespace certificate_transparency {
class TreeStateTracker;
}

namespace chrome {
class TestingIOThreadState;
}

namespace chrome_browser_net {
class DnsProbeService;
}

namespace data_usage {
class DataUseAggregator;
}

namespace data_use_measurement {
class ChromeDataUseAscriber;
}

namespace extensions {
class EventRouterForwarder;
}

namespace net {
class CTLogVerifier;
class HostMappingRules;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpAuthPreferences;
class LoggingNetworkChangeObserver;
class NetworkQualityEstimator;
class ProxyConfigService;
class RTTAndThroughputEstimatesObserver;
class SSLConfigService;
class URLRequestContext;
class URLRequestContextGetter;

namespace ct {
class STHObserver;
}

}  // namespace net

namespace net_log {
class ChromeNetLog;
}

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
    class SystemRequestContextLeakChecker {
     public:
      explicit SystemRequestContextLeakChecker(Globals* globals);
      ~SystemRequestContextLeakChecker();

     private:
      Globals* const globals_;
    };

    Globals();
    ~Globals();

    // Ascribes all data use in Chrome to a source, such as page loads.
    std::unique_ptr<data_use_measurement::ChromeDataUseAscriber>
        data_use_ascriber;
    // Global aggregator of data use. It must outlive the
    // |system_network_delegate|.
    std::unique_ptr<data_usage::DataUseAggregator> data_use_aggregator;
#if defined(OS_ANDROID)
    // An external observer of data use.
    std::unique_ptr<chrome::android::ExternalDataUseObserver>
        external_data_use_observer;
#endif  // defined(OS_ANDROID)
    std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs;
    std::unique_ptr<net::HttpAuthPreferences> http_auth_preferences;
    std::unique_ptr<net::URLRequestContext> system_request_context;
    SystemRequestContextLeakChecker system_request_context_leak_checker;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<extensions::EventRouterForwarder>
        extension_event_router_forwarder;
#endif
    std::unique_ptr<net::HostMappingRules> host_mapping_rules;
    std::unique_ptr<net::NetworkQualityEstimator> network_quality_estimator;
    std::unique_ptr<net::RTTAndThroughputEstimatesObserver>
        network_quality_observer;

    // NetErrorTabHelper uses |dns_probe_service| to send DNS probes when a
    // main frame load fails with a DNS error in order to provide more useful
    // information to the renderer so it can show a more specific error page.
    std::unique_ptr<chrome_browser_net::DnsProbeService> dns_probe_service;

    // Enables Brotli Content-Encoding support
    bool enable_brotli;
  };

  // |net_log| must either outlive the IOThread or be NULL.
  IOThread(PrefService* local_state,
           policy::PolicyService* policy_service,
           net_log::ChromeNetLog* net_log,
           extensions::EventRouterForwarder* extension_event_router_forwarder);

  ~IOThread() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Can only be called on the IO thread.
  Globals* globals();

  // Allows overriding Globals in tests where IOThread::Init() and
  // IOThread::CleanUp() are not called.  This allows for injecting mocks into
  // IOThread global objects.
  void SetGlobalsForTesting(Globals* globals);

  net_log::ChromeNetLog* net_log();

  // Handles changing to On The Record mode, discarding confidential data.
  void ChangedToOnTheRecord();

  // Returns a getter for the URLRequestContext.  Only called on the UI thread.
  net::URLRequestContextGetter* system_url_request_context_getter();

  // Clears the host cache. Intended to be used to prevent exposing recently
  // visited sites on about:net-internals/#dns and about:dns pages.  Must be
  // called on the IO thread. If |host_filter| is not null, only hosts matched
  // by it are deleted from the cache.
  void ClearHostCache(
      const base::Callback<bool(const std::string&)>& host_filter);

  const net::HttpNetworkSession::Params& NetworkSessionParams() const;

  // Dynamically disables QUIC for HttpNetworkSessions owned by io_thread, and
  // to HttpNetworkSession::Params which are used for the creation of new
  // HttpNetworkSessions. Not that re-enabling Quic dynamically is not
  // supported for simplicity and requires a browser restart.
  void DisableQuic();

  base::TimeTicks creation_time() const;

  // Returns the callback for updating data use prefs.
  metrics::UpdateUsagePrefCallbackType GetMetricsDataUseForwarder();

  // Registers the |observer| for new STH notifications.
  void RegisterSTHObserver(net::ct::STHObserver* observer);

  // Un-registers the |observer|.
  void UnregisterSTHObserver(net::ct::STHObserver* observer);

  // Returns true if the indicated proxy resolution features are
  // enabled. These features are controlled through
  // preferences/policy/commandline.
  //
  // For a description of what these features are, and how they are
  // configured, see the comments in pref_names.cc for
  // |kQuickCheckEnabled| and |kPacHttpsUrlStrippingEnabled
  // respectively.
  bool WpadQuickCheckEnabled() const;
  bool PacHttpsUrlStrippingEnabled() const;

 private:
  friend class test::IOThreadPeer;
  friend class chrome::TestingIOThreadState;

  // BrowserThreadDelegate implementation, runs on the IO thread.
  // This handles initialization and destruction of state that must
  // live on the IO thread.
  void Init() override;
  void CleanUp() override;

  std::unique_ptr<net::HttpAuthHandlerFactory> CreateDefaultAuthHandlerFactory(
      net::HostResolver* host_resolver);

  // Returns an SSLConfigService instance.
  net::SSLConfigService* GetSSLConfigService();

  void ChangedToOnTheRecordOnIOThread();

  void UpdateDnsClientEnabled();
  void UpdateServerWhitelist();
  void UpdateDelegateWhitelist();
  void UpdateAndroidAuthNegotiateAccountType();
  void UpdateNegotiateDisableCnameLookup();
  void UpdateNegotiateEnablePort();

  extensions::EventRouterForwarder* extension_event_router_forwarder() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return extension_event_router_forwarder_;
#else
    return NULL;
#endif
  }
  void ConstructSystemRequestContext();

  // Parse command line flags and use components/network_session_configurator to
  // configure |params|.
  static void ConfigureParamsFromFieldTrialsAndCommandLine(
      const base::CommandLine& command_line,
      bool is_quic_allowed_by_policy,
      bool http_09_on_non_default_ports_enabled,
      net::HttpNetworkSession::Params* params);

  // The NetLog is owned by the browser process, to allow logging from other
  // threads during shutdown, but is used most frequently on the IOThread.
  net_log::ChromeNetLog* net_log_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
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

  net::HttpNetworkSession::Params session_params_;

  // Observer that logs network changes to the ChromeNetLog.
  std::unique_ptr<net::LoggingNetworkChangeObserver> network_change_observer_;

  std::unique_ptr<certificate_transparency::TreeStateTracker> ct_tree_tracker_;

  BooleanPrefMember system_enable_referrers_;

  BooleanPrefMember dns_client_enabled_;

  BooleanPrefMember quick_check_enabled_;

  BooleanPrefMember pac_https_url_stripping_enabled_;

  // Store HTTP Auth-related policies in this thread.
  // TODO(aberent) Make the list of auth schemes a PrefMember, so that the
  // policy can change after startup (https://crbug/549273).
  std::string auth_schemes_;
  BooleanPrefMember negotiate_disable_cname_lookup_;
  BooleanPrefMember negotiate_enable_port_;
  StringPrefMember auth_server_whitelist_;
  StringPrefMember auth_delegate_whitelist_;

#if defined(OS_ANDROID)
  StringPrefMember auth_android_negotiate_account_type_;
#endif
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  // No PrefMember for the GSSAPI library name, since changing it after startup
  // requires unloading the existing GSSAPI library, which could cause all sorts
  // of problems for, for example, active Negotiate transactions.
  std::string gssapi_library_name_;
#endif

#if defined(OS_CHROMEOS)
  bool allow_gssapi_library_load_;
#endif

  // This is an instance of the default SSLConfigServiceManager for the current
  // platform and it gets SSL preferences from local_state object.
  std::unique_ptr<ssl_config::SSLConfigServiceManager>
      ssl_config_service_manager_;

  // These member variables are initialized by a task posted to the IO thread,
  // which gets posted by calling certain member functions of IOThread.
  std::unique_ptr<net::ProxyConfigService> system_proxy_config_service_;

  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_refptr<net::URLRequestContextGetter>
      system_url_request_context_getter_;

  // True if QUIC is allowed by policy.
  bool is_quic_allowed_by_policy_;

  // True if HTTP/0.9 is allowed on non-default ports by policy.
  bool http_09_on_non_default_ports_enabled_;

  const base::TimeTicks creation_time_;

  base::WeakPtrFactory<IOThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThread);
};

#endif  // CHROME_BROWSER_IO_THREAD_H_

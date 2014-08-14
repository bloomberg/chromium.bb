// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher_delegate.h"

class PrefService;

namespace net {
class HostPortPair;
class HttpNetworkSession;
class HttpResponseHeaders;
class URLFetcher;
class URLRequestContextGetter;
}

namespace data_reduction_proxy {

// The number of days of bandwidth usage statistics that are tracked.
const unsigned int kNumDaysInHistory = 60;

// The number of days of bandwidth usage statistics that are presented.
const unsigned int kNumDaysInHistorySummary = 30;

COMPILE_ASSERT(kNumDaysInHistorySummary <= kNumDaysInHistory,
               DataReductionProxySettings_summary_too_long);

// Values of the UMA DataReductionProxy.StartupState histogram.
// This enum must remain synchronized with DataReductionProxyStartupState
// in metrics/histograms/histograms.xml.
enum ProxyStartupState {
  PROXY_NOT_AVAILABLE = 0,
  PROXY_DISABLED,
  PROXY_ENABLED,
  PROXY_STARTUP_STATE_COUNT,
};

// Values of the UMA DataReductionProxy.ProbeURL histogram.
// This enum must remain synchronized with
// DataReductionProxyProbeURLFetchResult in metrics/histograms/histograms.xml.
// TODO(marq): Rename these histogram buckets with s/DISABLED/RESTRICTED/, so
//     their names match the behavior they track.
enum ProbeURLFetchResult {
  // The probe failed because the Internet was disconnected.
  INTERNET_DISCONNECTED = 0,

  // The probe failed for any other reason, and as a result, the proxy was
  // disabled.
  FAILED_PROXY_DISABLED,

  // The probe failed, but the proxy was already restricted.
  FAILED_PROXY_ALREADY_DISABLED,

  // The probe succeeded, and as a result the proxy was restricted.
  SUCCEEDED_PROXY_ENABLED,

  // The probe succeeded, but the proxy was already restricted.
  SUCCEEDED_PROXY_ALREADY_ENABLED,

  // This must always be last.
  PROBE_URL_FETCH_RESULT_COUNT
};

// Central point for configuring the data reduction proxy.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
// TODO(marq): Convert this to be a KeyedService with an
// associated factory class, and refactor the Java call sites accordingly.
class DataReductionProxySettings
    : public net::URLFetcherDelegate,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  typedef std::vector<long long> ContentLengthList;

  static bool IsProxyKeySetOnCommandLine();

  DataReductionProxySettings(DataReductionProxyParams* params);
  virtual ~DataReductionProxySettings();

  DataReductionProxyParams* params() const {
    return params_.get();
  }

  // Initializes the data reduction proxy with profile and local state prefs,
  // and a |UrlRequestContextGetter| for canary probes. The caller must ensure
  // that all parameters remain alive for the lifetime of the
  // |DataReductionProxySettings| instance.
  void InitDataReductionProxySettings(
      PrefService* prefs,
      PrefService* local_state_prefs,
      net::URLRequestContextGetter* url_request_context_getter);

  // Initializes the data reduction proxy with profile and local state prefs,
  // a |UrlRequestContextGetter| for canary probes, and a proxy configurator.
  // The caller must ensure that all parameters remain alive for the lifetime of
  // the |DataReductionProxySettings| instance.
  // TODO(marq): Remove when iOS supports the new interface above.
  void InitDataReductionProxySettings(
      PrefService* prefs,
      PrefService* local_state_prefs,
      net::URLRequestContextGetter* url_request_context_getter,
      DataReductionProxyConfigurator* configurator);

  // Sets the |on_data_reduction_proxy_enabled_| callback and runs to register
  // the DataReductionProxyEnabled synthetic field trial.
  void SetOnDataReductionEnabledCallback(
      const base::Callback<void(bool)>& on_data_reduction_proxy_enabled);

  // Sets the logic the embedder uses to set the networking configuration that
  // causes traffic to be proxied.
  void SetProxyConfigurator(
      DataReductionProxyConfigurator* configurator);

  // Returns true if the proxy is enabled.
  bool IsDataReductionProxyEnabled();

  // Returns true if the alternative proxy is enabled.
  bool IsDataReductionProxyAlternativeEnabled() const;

  // Returns true if the proxy is managed by an adminstrator's policy.
  bool IsDataReductionProxyManaged();

  // Enables or disables the data reduction proxy. If a probe URL is available,
  // and a probe request fails at some point, the proxy won't be used until a
  // probe succeeds.
  void SetDataReductionProxyEnabled(bool enabled);

  // Enables or disables the alternative data reduction proxy configuration.
  void SetDataReductionProxyAlternativeEnabled(bool enabled);

  // Returns the time in microseconds that the last update was made to the
  // daily original and received content lengths.
  int64 GetDataReductionLastUpdateTime();

  // Returns a vector containing the total size of all HTTP content that was
  // received over the last |kNumDaysInHistory| before any compression by the
  // data reduction proxy. Each element in the vector contains one day of data.
  ContentLengthList GetDailyOriginalContentLengths();

  // Returns aggregate received and original content lengths over the specified
  // number of days, as well as the time these stats were last updated.
  void GetContentLengths(unsigned int days,
                         int64* original_content_length,
                         int64* received_content_length,
                         int64* last_update_time);

  // Records that the data reduction proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Returns whether the data reduction proxy is unreachable. Returns true
  // if no request has successfully completed through proxy, even though atleast
  // some of them should have.
  bool IsDataReductionProxyUnreachable();

  // Returns an vector containing the aggregate received HTTP content in the
  // last |kNumDaysInHistory| days.
  ContentLengthList GetDailyReceivedContentLengths();

  ContentLengthList GetDailyContentLengths(const char* pref_name);

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 protected:
  void InitPrefMembers();

  // Returns a fetcher for the probe to check if OK for the proxy to use SPDY.
  // Virtual for testing.
  virtual net::URLFetcher* GetURLFetcherForAvailabilityCheck();

  // Returns a fetcher to warm up the connection to the data reduction proxy.
  // Virtual for testing.
  virtual net::URLFetcher* GetURLFetcherForWarmup();

  // Virtualized for unit test support.
  virtual PrefService* GetOriginalProfilePrefs();
  virtual PrefService* GetLocalStatePrefs();

  // Sets the proxy configs, enabling or disabling the proxy according to
  // the value of |enabled| and |alternative_enabled|. Use the alternative
  // configuration only if |enabled| and |alternative_enabled| are true. If
  // |restricted| is true, only enable the fallback proxy. |at_startup| is true
  // when this method is called from InitDataReductionProxySettings.
  virtual void SetProxyConfigs(bool enabled,
                               bool alternative_enabled,
                               bool restricted,
                               bool at_startup);

  // Metrics method. Subclasses should override if they wish to provide
  // alternatives.
  virtual void RecordDataReductionInit();

  virtual void AddDefaultProxyBypassRules();

  // Writes a warning to the log that is used in backend processing of
  // customer feedback. Virtual so tests can mock it for verification.
  virtual void LogProxyState(bool enabled, bool restricted, bool at_startup);

  // Virtualized for mocking. Records UMA containing the result of requesting
  // the probe URL.
  virtual void RecordProbeURLFetchResult(
      data_reduction_proxy::ProbeURLFetchResult result);

  // Virtualized for mocking. Records UMA specifying whether the proxy was
  // enabled or disabled at startup.
  virtual void RecordStartupState(
      data_reduction_proxy::ProxyStartupState state);

  // Virtualized for mocking. Returns the list of network interfaces in use.
  virtual void GetNetworkList(net::NetworkInterfaceList* interfaces,
                              int policy);

  DataReductionProxyConfigurator* configurator() {
    return configurator_;
  }

  // Reset params for tests.
  void ResetParamsForTest(DataReductionProxyParams* params);

 private:
  friend class DataReductionProxySettingsTestBase;
  friend class DataReductionProxySettingsTest;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestAuthenticationInit);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestAuthHashGeneration);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestAuthHashGenerationWithOriginSetViaSwitch);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestResetDataReductionStatistics);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestIsProxyEnabledOrManaged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestGetDailyContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestMaybeActivateDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestOnIPAddressChanged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestOnProxyEnabledPrefChange);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOn);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOff);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestBypassList);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           CheckInitMetricsWhenNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestSetProxyConfigs);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestSetProxyConfigsHoldback);

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  void OnProxyEnabledPrefChange();
  void OnProxyAlternativeEnabledPrefChange();

  void ResetDataReductionStatistics();

  void MaybeActivateDataReductionProxy(bool at_startup);

  // Requests the proxy probe URL, if one is set.  If unable to do so, disables
  // the proxy, if enabled. Otherwise enables the proxy if disabled by a probe
  // failure.
  void ProbeWhetherDataReductionProxyIsAvailable();

  // Warms the connection to the data reduction proxy.
  void WarmProxyConnection();

  // Disables use of the data reduction proxy on VPNs. Returns true if the
  // data reduction proxy has been disabled.
  bool DisableIfVPN();

  // Generic method to get a URL fetcher.
  net::URLFetcher* GetBaseURLFetcher(const GURL& gurl, int load_flags);

  std::string key_;
  bool restricted_by_carrier_;
  bool enabled_by_user_;
  bool disabled_on_vpn_;
  bool unreachable_;

  scoped_ptr<net::URLFetcher> fetcher_;
  scoped_ptr<net::URLFetcher> warmup_fetcher_;

  BooleanPrefMember spdy_proxy_auth_enabled_;
  BooleanPrefMember data_reduction_proxy_alternative_enabled_;

  PrefService* prefs_;
  PrefService* local_state_prefs_;

  net::URLRequestContextGetter* url_request_context_getter_;

  base::Callback<void(bool)> on_data_reduction_proxy_enabled_;

  DataReductionProxyConfigurator* configurator_;

  base::ThreadChecker thread_checker_;

  scoped_ptr<DataReductionProxyParams> params_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettings);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher_delegate.h"

class PrefService;

namespace net {
class AuthChallengeInfo;
class HostPortPair;
class HttpAuthCache;
class HttpNetworkSession;
class URLFetcher;
}

namespace spdyproxy {

// The number of days of bandwidth usage statistics that are tracked.
const unsigned int kNumDaysInHistory = 60;

// The number of days of bandwidth usage statistics that are presented.
const unsigned int kNumDaysInHistorySummary = 30;

COMPILE_ASSERT(kNumDaysInHistorySummary <= kNumDaysInHistory,
               DataReductionProxySettings_summary_too_long);

}  // namespace spdyproxy

// Central point for configuring the data reduction proxy.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
// TODO(marq): Convert this to be a BrowserContextKeyedService with an
// associated factory class, and refactor the Java call sites accordingly.
class DataReductionProxySettings
    : public net::URLFetcherDelegate,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  typedef std::vector<long long> ContentLengthList;
  // TODO(marq): Consider instead using a std::pair instead of a vector.
  typedef std::vector<GURL> DataReductionProxyList;

  DataReductionProxySettings();
  virtual ~DataReductionProxySettings();

  void InitDataReductionProxySettings();

  // If proxy authentication is compiled in, pre-cache authentication
  // keys for all configured proxies in |session|.
  static void InitDataReductionProxySession(net::HttpNetworkSession* session);

  // Add a host pattern to bypass. This should follow the same syntax used
  // in net::ProxyBypassRules; that is, a hostname pattern, a hostname suffix
  // pattern, an IP literal, a CIDR block, or the magic string '<local>'.
  // Bypass settings persist for the life of this object and are applied
  // each time the proxy is enabled, but are not updated while it is enabled.
  void AddHostPatternToBypass(const std::string& pattern);

  // Add a URL pattern to bypass the proxy. The base implementation strips
  // everything in |pattern| after the first single slash and then treats it
  // as a hostname pattern. Subclasses may implement other semantics.
  virtual void AddURLPatternToBypass(const std::string& pattern);

  // Returns true if the data reduction proxy is allowed to be used on this
  // instance of Chrome. This could return false, for example, if this instance
  // is not part of the field trial, or if the proxy name is not configured
  // via gyp.
  static bool IsDataReductionProxyAllowed();

  // Returns true if a screen promoting the data reduction proxy is allowed to
  // be shown. Logic that decides when to show the promo should check its
  // availability. This would return false if not part of a separate field
  // trial that governs the use of the promotion.
  static bool IsDataReductionProxyPromoAllowed();

  // Returns the URL of the data reduction proxy.
  static std::string GetDataReductionProxyOrigin();

  // Returns the URL of the fallback data reduction proxy.
  static std::string GetDataReductionProxyFallback();

  // Returns a vector of GURLs for all configured proxies.
  static DataReductionProxyList GetDataReductionProxies();

  // Returns true if |auth_info| represents an authentication challenge from
  // a compatible, configured proxy.
  bool IsAcceptableAuthChallenge(net::AuthChallengeInfo* auth_info);

  // Returns a UTF16 string suitable for use as an authentication token in
  // response to the challenge represented by |auth_info|. If the token can't
  // be correctly generated for |auth_info|, returns an empty UTF16 string.
  base::string16 GetTokenForAuthChallenge(net::AuthChallengeInfo* auth_info);

  // Returns true if the proxy is enabled.
  bool IsDataReductionProxyEnabled();

  // Returns true if the proxy is managed by an adminstrator's policy.
  bool IsDataReductionProxyManaged();

  // Enables or disables the data reduction proxy. If a probe URL is available,
  // and a probe request fails at some point, the proxy won't be used until a
  // probe succeeds.
  void SetDataReductionProxyEnabled(bool enabled);

  // Returns the time in microseconds that the last update was made to the
  // daily original and received content lengths.
  int64 GetDataReductionLastUpdateTime();

  // Returns a vector containing the total size of all HTTP content that was
  // received over the last |kNumDaysInHistory| before any compression by the
  // data reduction proxy. Each element in the vector contains one day of data.
  ContentLengthList GetDailyOriginalContentLengths();

  // Returns an vector containing the aggregate received HTTP content in the
  // last |kNumDaysInHistory| days.
  ContentLengthList GetDailyReceivedContentLengths();

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 protected:
  void InitPrefMembers();

  virtual net::URLFetcher* GetURLFetcher();

  // Virtualized for unit test support.
  virtual PrefService* GetOriginalProfilePrefs();
  virtual PrefService* GetLocalStatePrefs();

  void GetContentLengths(unsigned int days,
                         int64* original_content_length,
                         int64* received_content_length,
                         int64* last_update_time);
  ContentLengthList GetDailyContentLengths(const char* pref_name);

  // Sets the proxy configs, enabling or disabling the proxy according to
  // the value of |enabled|. |at_startup| is true when this method is called
  // from InitDataReductionProxySettings.
  virtual void SetProxyConfigs(bool enabled, bool at_startup);

  // Metrics methods. Subclasses should override if they wish to provide
  // alternate methods.
  virtual void RecordDataReductionInit();

  virtual void AddDefaultProxyBypassRules();

  // Writes a warning to the log that is used in backend processing of
  // customer feedback. Virtual so tests can mock it for verification.
  virtual void LogProxyState(bool enabled, bool at_startup);

  bool HasTurnedOn() { return has_turned_on_; }
  bool HasTurnedOff() { return has_turned_off_; }
  // Note that these flags may only be toggled to true, never back to false.
  void SetHasTurnedOn() { has_turned_on_ = true; }
  void SetHasTurnedOff() { has_turned_off_ = true; }

  // Accessor for unit tests.
  std::vector<std::string> BypassRules() { return bypass_rules_;}

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

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  // Underlying implementation of InitDataReductionProxySession(), factored
  // out to be testable without creating a full HttpNetworkSession.
  static void InitDataReductionAuthentication(net::HttpAuthCache* auth_cache);

  void OnProxyEnabledPrefChange();

  void ResetDataReductionStatistics();

  void MaybeActivateDataReductionProxy(bool at_startup);

  // Requests the proxy probe URL, if one is set.  If unable to do so, disables
  // the proxy, if enabled. Otherwise enables the proxy if disabled by a probe
  // failure.
  void ProbeWhetherDataReductionProxyIsAvailable();
  std::string GetProxyCheckURL();

  // Returns a UTF16 string that's the hash of the configured authentication
  // key and |salt|. Returns an empty UTF16 string if no key is configured or
  // the data reduction proxy feature isn't available.
  static base::string16 AuthHashForSalt(int64 salt);

  std::vector<std::string> bypass_rules_;

  // Indicate whether a user has turned on the data reduction proxy previously
  // in this session.
  bool has_turned_on_;

  // Indicate whether a user has turned off the data reduction proxy previously
  // in this session.
  bool has_turned_off_;

  bool disabled_by_carrier_;
  bool enabled_by_user_;

  scoped_ptr<net::URLFetcher> fetcher_;
  BooleanPrefMember spdy_proxy_auth_enabled_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettings);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_H_

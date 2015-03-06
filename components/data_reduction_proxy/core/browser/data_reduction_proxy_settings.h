// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/threading/thread_checker.h"
#include "url/gurl.h"

class PrefService;

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxyEventStore;
class DataReductionProxyIOData;
class DataReductionProxyService;
class DataReductionProxyStatisticsPrefs;

// The number of days of bandwidth usage statistics that are tracked.
const unsigned int kNumDaysInHistory = 60;

// The header used to request a data reduction proxy pass through. When a
// request is sent to the data reduction proxy with this header, it will respond
// with the original uncompressed response.
extern const char kDataReductionPassThroughHeader[];

// The number of days of bandwidth usage statistics that are presented.
const unsigned int kNumDaysInHistorySummary = 30;

static_assert(kNumDaysInHistorySummary <= kNumDaysInHistory,
              "kNumDaysInHistorySummary should be no larger than "
              "kNumDaysInHistory");

// Values of the UMA DataReductionProxy.StartupState histogram.
// This enum must remain synchronized with DataReductionProxyStartupState
// in metrics/histograms/histograms.xml.
enum ProxyStartupState {
  PROXY_NOT_AVAILABLE = 0,
  PROXY_DISABLED,
  PROXY_ENABLED,
  PROXY_STARTUP_STATE_COUNT,
};

// Central point for configuring the data reduction proxy.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
// TODO(marq): Convert this to be a KeyedService with an
// associated factory class, and refactor the Java call sites accordingly.
class DataReductionProxySettings {
 public:
  typedef std::vector<long long> ContentLengthList;

  DataReductionProxySettings();
  virtual ~DataReductionProxySettings();

  // Initializes the data reduction proxy with profile prefs and a
  // |DataReductionProxyIOData|. The caller must ensure that all parameters
  // remain alive for the lifetime of the |DataReductionProxySettings| instance.
  void InitDataReductionProxySettings(
      PrefService* prefs,
      DataReductionProxyIOData* io_data,
      scoped_ptr<DataReductionProxyService> data_reduction_proxy_service);

  base::WeakPtr<DataReductionProxyStatisticsPrefs> statistics_prefs();

  // Sets the |on_data_reduction_proxy_enabled_| callback and runs to register
  // the DataReductionProxyEnabled synthetic field trial.
  void SetOnDataReductionEnabledCallback(
      const base::Callback<void(bool)>& on_data_reduction_proxy_enabled);

  // Returns true if the proxy is enabled.
  bool IsDataReductionProxyEnabled() const;

  // Returns true if the proxy can be used for the given url. This method does
  // not take into account the proxy config or proxy retry list, so it can
  // return true even when the proxy will not be used. Specifically, if
  // another proxy configuration overrides use of data reduction proxy, or
  // if data reduction proxy is in proxy retry list, then data reduction proxy
  // will not be used, but this method will still return true. If this method
  // returns false, then we are guaranteed that data reduction proxy will not be
  // used.
  bool CanUseDataReductionProxy(const GURL& url) const;

  // Returns true if the alternative proxy is enabled.
  bool IsDataReductionProxyAlternativeEnabled() const;

  // Returns true if the proxy is managed by an adminstrator's policy.
  bool IsDataReductionProxyManaged();

  // Enables or disables the data reduction proxy.
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

  // Configures data reduction proxy. |at_startup| is true when this method is
  // called in response to creating or loading a new profile.
  void MaybeActivateDataReductionProxy(bool at_startup);

  // Returns the event store being used. May be null if
  // InitDataReductionProxySettings has not been called.
  DataReductionProxyEventStore* GetEventStore() const {
    return event_store_;
  }

  // Returns true if the data reduction proxy configuration may be used.
  bool Allowed() const {
    return allowed_;
  }

  // Returns true if the alternative data reduction proxy configuration may be
  // used.
  bool AlternativeAllowed() const {
    return alternative_allowed_;
  }

  // Returns true if the data reduction proxy promo may be shown.
  // This is idependent of whether the data reduction proxy is allowed.
  bool PromoAllowed() const {
    return promo_allowed_;
  }

  DataReductionProxyService* data_reduction_proxy_service() {
    return data_reduction_proxy_service_.get();
  }

  // Returns the |DataReductionProxyConfig| being used. May be null if
  // InitDataReductionProxySettings has not been called.
  DataReductionProxyConfig* Config() const {
    return config_;
  }

  // Permits changing the underlying |DataReductionProxyConfig| without running
  // the initialization loop.
  void ResetConfigForTest(DataReductionProxyConfig* config) {
    config_ = config;
  }

 protected:
  void InitPrefMembers();

  void UpdateConfigValues();

  // Virtualized for unit test support.
  virtual PrefService* GetOriginalProfilePrefs();

  // Metrics method. Subclasses should override if they wish to provide
  // alternatives.
  virtual void RecordDataReductionInit();

  // Virtualized for mocking. Records UMA specifying whether the proxy was
  // enabled or disabled at startup.
  virtual void RecordStartupState(
      data_reduction_proxy::ProxyStartupState state);

 private:
  friend class DataReductionProxySettingsTestBase;
  friend class DataReductionProxySettingsTest;
  friend class DataReductionProxyTestContext;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestResetDataReductionStatistics);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestIsProxyEnabledOrManaged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestCanUseDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest, TestContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestGetDailyContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestMaybeActivateDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestOnProxyEnabledPrefChange);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOn);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOff);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           CheckInitMetricsWhenNotAllowed);

  void OnProxyEnabledPrefChange();
  void OnProxyAlternativeEnabledPrefChange();

  void ResetDataReductionStatistics();

  bool unreachable_;

  // The following values are cached in order to access the values on the
  // correct thread.
  bool allowed_;
  bool alternative_allowed_;
  bool promo_allowed_;

  BooleanPrefMember spdy_proxy_auth_enabled_;
  BooleanPrefMember data_reduction_proxy_alternative_enabled_;

  scoped_ptr<DataReductionProxyService> data_reduction_proxy_service_;

  PrefService* prefs_;

  // The caller must ensure that the |event_store_| outlives this instance.
  DataReductionProxyEventStore* event_store_;

  // The caller must ensure that the |config_| outlives this instance.
  DataReductionProxyConfig* config_;

  base::Callback<void(bool)> on_data_reduction_proxy_enabled_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettings);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

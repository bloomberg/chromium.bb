// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_metrics.h"

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"

namespace data_reduction_proxy {

namespace {

// A bypass delay more than this is treated as a long delay.
const int kLongBypassDelayInSeconds = 30 * 60;

// Increments an int64, stored as a string, in a ListPref at the specified
// index.  The value must already exist and be a string representation of a
// number.
void AddInt64ToListPref(size_t index,
                        int64 length,
                        base::ListValue* list_update) {
  int64 value = 0;
  std::string old_string_value;
  bool rv = list_update->GetString(index, &old_string_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(old_string_value, &value);
    DCHECK(rv);
  }
  value += length;
  list_update->Set(index, new base::StringValue(base::Int64ToString(value)));
}

int64 ListPrefInt64Value(const base::ListValue& list_update, size_t index) {
  std::string string_value;
  if (!list_update.GetString(index, &string_value)) {
    NOTREACHED();
    return 0;
  }

  int64 value = 0;
  bool rv = base::StringToInt64(string_value, &value);
  DCHECK(rv);
  return value;
}

// Report UMA metrics for daily data reductions.
void RecordDailyContentLengthHistograms(
    int64 original_length,
    int64 received_length,
    int64 original_length_with_data_reduction_enabled,
    int64 received_length_with_data_reduction_enabled,
    int64 original_length_via_data_reduction_proxy,
    int64 received_length_via_data_reduction_proxy,
    int64 https_length_with_data_reduction_enabled,
    int64 short_bypass_length_with_data_reduction_enabled,
    int64 long_bypass_length_with_data_reduction_enabled,
    int64 unknown_length_with_data_reduction_enabled) {
  // Report daily UMA only for days having received content.
  if (original_length <= 0 || received_length <= 0)
    return;

  // Record metrics in KB.
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength", original_length >> 10);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength", received_length >> 10);
  int percent = 0;
  // UMA percentage cannot be negative.
  if (original_length > received_length) {
    percent = (100 * (original_length - received_length)) / original_length;
  }
  UMA_HISTOGRAM_PERCENTAGE("Net.DailyContentSavingPercent", percent);

  if (original_length_with_data_reduction_enabled <= 0 ||
      received_length_with_data_reduction_enabled <= 0) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength_DataReductionProxyEnabled",
      original_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled",
      received_length_with_data_reduction_enabled >> 10);

  int percent_data_reduction_proxy_enabled = 0;
  // UMA percentage cannot be negative.
  if (original_length_with_data_reduction_enabled >
      received_length_with_data_reduction_enabled) {
    percent_data_reduction_proxy_enabled =
        100 * (original_length_with_data_reduction_enabled -
               received_length_with_data_reduction_enabled) /
        original_length_with_data_reduction_enabled;
  }
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentSavingPercent_DataReductionProxyEnabled",
      percent_data_reduction_proxy_enabled);

  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled",
      (100 * received_length_with_data_reduction_enabled) / received_length);

  DCHECK_GE(https_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_Https",
      https_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_Https",
      (100 * https_length_with_data_reduction_enabled) / received_length);

  DCHECK_GE(short_bypass_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_ShortBypass",
      short_bypass_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_ShortBypass",
      ((100 * short_bypass_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(long_bypass_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_LongBypass",
      long_bypass_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_LongBypass",
      ((100 * long_bypass_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(unknown_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_Unknown",
      unknown_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_Unknown",
      ((100 * unknown_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(original_length_via_data_reduction_proxy, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength_ViaDataReductionProxy",
      original_length_via_data_reduction_proxy >> 10);
  DCHECK_GE(received_length_via_data_reduction_proxy, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_ViaDataReductionProxy",
      received_length_via_data_reduction_proxy >> 10);
  int percent_via_data_reduction_proxy = 0;
  if (original_length_via_data_reduction_proxy >
      received_length_via_data_reduction_proxy) {
    percent_via_data_reduction_proxy =
        100 * (original_length_via_data_reduction_proxy -
               received_length_via_data_reduction_proxy) /
        original_length_via_data_reduction_proxy;
  }
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentSavingPercent_ViaDataReductionProxy",
      percent_via_data_reduction_proxy);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_ViaDataReductionProxy",
      (100 * received_length_via_data_reduction_proxy) / received_length);
}

// Ensure list has exactly |length| elements, either by truncating at the
// front, or appending "0"'s to the back.
void MaintainContentLengthPrefsWindow(base::ListValue* list, size_t length) {
  // Remove data for old days from the front.
  while (list->GetSize() > length)
    list->Remove(0, NULL);
  // Newly added lists are empty. Add entries to back to fill the window,
  // each initialized to zero.
  while (list->GetSize() < length)
    list->AppendString(base::Int64ToString(0));
  DCHECK_EQ(length, list->GetSize());
}

// DailyContentLengthUpdate maintains a data saving pref. The pref is a list
// of |kNumDaysInHistory| elements of daily total content lengths for the past
// |kNumDaysInHistory| days.
class DailyContentLengthUpdate {
 public:
  DailyContentLengthUpdate(
      const char* pref,
      PrefService* pref_service)
      : update_(pref_service, pref) {
  }

  void UpdateForDataChange(int days_since_last_update) {
    // New empty lists may have been created. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_.Get(), kNumDaysInHistory);
    if (days_since_last_update) {
      MaintainContentLengthPrefForDateChange(days_since_last_update);
    }
  }

  // Update the lengths for the current day.
  void Add(int content_length) {
    AddInt64ToListPref(kNumDaysInHistory - 1, content_length, update_.Get());
  }

  int64 GetListPrefValue(size_t index) {
    return ListPrefInt64Value(*update_, index);
  }

 private:
  // Update the list for date change and ensure the list has exactly |length|
  // elements. The last entry in the list will be for the current day after
  // the update.
  void MaintainContentLengthPrefForDateChange(int days_since_last_update) {
    if (days_since_last_update == -1) {
      // The system may go backwards in time by up to a day for legitimate
      // reasons, such as with changes to the time zone. In such cases, we
      // keep adding to the current day.
      // Note: we accept the fact that some reported data is shifted to
      // the adjacent day if users travel back and forth across time zones.
      days_since_last_update = 0;
    } else if (days_since_last_update < -1) {
      // Erase all entries if the system went backwards in time by more than
      // a day.
      update_->Clear();

      days_since_last_update = kNumDaysInHistory;
    }
    DCHECK_GE(days_since_last_update, 0);

    // Add entries for days since last update event. This will make the
    // lists longer than kNumDaysInHistory. The additional items will be cut off
    // from the head of the lists by |MaintainContentLengthPrefsWindow|, below.
    for (int i = 0;
         i < days_since_last_update && i < static_cast<int>(kNumDaysInHistory);
         ++i) {
      update_->AppendString(base::Int64ToString(0));
    }

    // Entries for new days may have been appended. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_.Get(), kNumDaysInHistory);
  }

  ListPrefUpdate update_;
};

// DailyDataSavingUpdate maintains a pair of data saving prefs, original_update_
// and received_update_. pref_original is a list of |kNumDaysInHistory| elements
// of daily total original content lengths for the past |kNumDaysInHistory|
// days. pref_received is the corresponding list of the daily total received
// content lengths.
class DailyDataSavingUpdate {
 public:
  DailyDataSavingUpdate(
      const char* pref_original,
      const char* pref_received,
      PrefService* pref_service)
      : original_(pref_original, pref_service),
        received_(pref_received, pref_service) {
  }

  void UpdateForDataChange(int days_since_last_update) {
    original_.UpdateForDataChange(days_since_last_update);
    received_.UpdateForDataChange(days_since_last_update);
  }

  // Update the lengths for the current day.
  void Add(int original_content_length, int received_content_length) {
    original_.Add(original_content_length);
    received_.Add(received_content_length);
  }

  int64 GetOriginalListPrefValue(size_t index) {
    return original_.GetListPrefValue(index);
  }
  int64 GetReceivedListPrefValue(size_t index) {
    return received_.GetListPrefValue(index);
  }

 private:
  DailyContentLengthUpdate original_;
  DailyContentLengthUpdate received_;
};

// Returns true if the request is bypassed by all configured data reduction
// proxies. It returns the bypass delay in delay_seconds (if not NULL). If
// the request is bypassed by more than one proxy, delay_seconds returns
// shortest delay.
bool IsBypassRequest(const net::URLRequest* request, int64* delay_seconds) {
  // TODO(bengr): Add support for other data reduction proxy configurations.
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  DataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed);
  DataReductionProxyParams::DataReductionProxyList proxies =
      params.GetAllowedProxies();
  if (proxies.size() == 0)
    return false;

  if (request == NULL || request->context() == NULL ||
      request->context()->proxy_service() == NULL) {
    return false;
  }

  const net::ProxyRetryInfoMap& retry_map =
      request->context()->proxy_service()->proxy_retry_info();
  if (retry_map.size() == 0)
    return false;

  int64 shortest_delay = 0;
  // The request is bypassed if all configured proxies are in the retry map.
  for (size_t i = 0; i < proxies.size(); ++i) {
    std::string proxy = net::HostPortPair::FromURL(proxies[i]).ToString();
    // The retry list has the scheme prefix for https but not for http.
    if (proxies[i].SchemeIs("https"))
      proxy = std::string("https://") + proxy;

    net::ProxyRetryInfoMap::const_iterator found = retry_map.find(proxy);
    if (found == retry_map.end())
      return false;
    if (shortest_delay == 0 ||
        shortest_delay > found->second.current_delay.InSeconds()) {
      shortest_delay = found->second.current_delay.InSeconds();
    }
  }
  if (delay_seconds != NULL)
    *delay_seconds = shortest_delay;
  return true;
#else
  return false;
#endif
}

}  // namespace

DataReductionProxyRequestType GetDataReductionProxyRequestType(
    const net::URLRequest* request) {
  if (request->url().SchemeIs("https"))
    return HTTPS;
  if (!request->url().SchemeIs("http")) {
    NOTREACHED();
    return UNKNOWN_TYPE;
  }
  int64 bypass_delay = 0;
  if (IsBypassRequest(request, &bypass_delay)) {
    return (bypass_delay > kLongBypassDelayInSeconds) ?
      LONG_BYPASS : SHORT_BYPASS;
  }
  if (request->response_info().headers &&
      HasDataReductionProxyViaHeader(request->response_info().headers)) {
    return VIA_DATA_REDUCTION_PROXY;
  }
  return UNKNOWN_TYPE;
}

int64 GetAdjustedOriginalContentLength(
    DataReductionProxyRequestType request_type,
    int64 original_content_length,
    int64 received_content_length) {
  // Since there was no indication of the original content length, presume
  // it is no different from the number of bytes read.
  if (original_content_length == -1 ||
      request_type != VIA_DATA_REDUCTION_PROXY) {
    return received_content_length;
  }
  return original_content_length;
}

void UpdateContentLengthPrefsForDataReductionProxy(
    int received_content_length,
    int original_content_length,
    bool with_data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    base::Time now, PrefService* prefs) {
  // TODO(bengr): Remove this check once the underlying cause of
  // http://crbug.com/287821 is fixed. For now, only continue if the current
  // year is reported as being between 1972 and 2970.
  base::TimeDelta time_since_unix_epoch = now - base::Time::UnixEpoch();
  const int kMinDaysSinceUnixEpoch = 365 * 2;  // 2 years.
  const int kMaxDaysSinceUnixEpoch = 365 * 1000;  // 1000 years.
  if (time_since_unix_epoch.InDays() < kMinDaysSinceUnixEpoch ||
      time_since_unix_epoch.InDays() > kMaxDaysSinceUnixEpoch) {
    return;
  }

  // Determine how many days it has been since the last update.
  int64 then_internal = prefs->GetInt64(
      data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate);

#if defined(OS_WIN)
  base::Time then_midnight = base::Time::FromInternalValue(then_internal);
  base::Time midnight =
      base::Time::FromInternalValue(
          (now.ToInternalValue() / base::Time::kMicrosecondsPerDay) *
              base::Time::kMicrosecondsPerDay);
#else
  // Local midnight could have been shifted due to time zone change.
  base::Time then_midnight =
      base::Time::FromInternalValue(then_internal).LocalMidnight();
  base::Time midnight = now.LocalMidnight();
#endif

  int days_since_last_update = (midnight - then_midnight).InDays();

  // Each day, we calculate the total number of bytes received and the total
  // size of all corresponding resources before any data-reducing recompression
  // is applied. These values are used to compute the data savings realized
  // by applying our compression techniques. Totals for the last
  // |kNumDaysInHistory| days are maintained.
  DailyDataSavingUpdate total(
      data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
      data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
      prefs);
  total.UpdateForDataChange(days_since_last_update);

  DailyDataSavingUpdate proxy_enabled(
      data_reduction_proxy::prefs::
          kDailyOriginalContentLengthWithDataReductionProxyEnabled,
      data_reduction_proxy::prefs::
          kDailyContentLengthWithDataReductionProxyEnabled,
      prefs);
  proxy_enabled.UpdateForDataChange(days_since_last_update);

  DailyDataSavingUpdate via_proxy(
      data_reduction_proxy::prefs::
          kDailyOriginalContentLengthViaDataReductionProxy,
      data_reduction_proxy::prefs::
          kDailyContentLengthViaDataReductionProxy,
      prefs);
  via_proxy.UpdateForDataChange(days_since_last_update);

  DailyContentLengthUpdate https(
      data_reduction_proxy::prefs::
          kDailyContentLengthHttpsWithDataReductionProxyEnabled,
      prefs);
  https.UpdateForDataChange(days_since_last_update);

  DailyContentLengthUpdate short_bypass(
      data_reduction_proxy::prefs::
          kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
      prefs);
  short_bypass.UpdateForDataChange(days_since_last_update);

  DailyContentLengthUpdate long_bypass(
      data_reduction_proxy::prefs::
          kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
      prefs);
  long_bypass.UpdateForDataChange(days_since_last_update);

  DailyContentLengthUpdate unknown(
      data_reduction_proxy::prefs::
          kDailyContentLengthUnknownWithDataReductionProxyEnabled,
      prefs);
  unknown.UpdateForDataChange(days_since_last_update);

  total.Add(original_content_length, received_content_length);
  if (with_data_reduction_proxy_enabled) {
    proxy_enabled.Add(original_content_length, received_content_length);
    // Ignore data source cases, if exist, when
    // "with_data_reduction_proxy_enabled == false"
    switch (request_type) {
      case VIA_DATA_REDUCTION_PROXY:
        via_proxy.Add(original_content_length, received_content_length);
        break;
      case HTTPS:
        https.Add(received_content_length);
        break;
      case SHORT_BYPASS:
        short_bypass.Add(received_content_length);
        break;
      case LONG_BYPASS:
        long_bypass.Add(received_content_length);
        break;
      case UNKNOWN_TYPE:
        unknown.Add(received_content_length);
        break;
    }
  }

  if (days_since_last_update) {
    // Record the last update time in microseconds in UTC.
    prefs->SetInt64(
        data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate,
        midnight.ToInternalValue());

    // A new day. Report the previous day's data if exists. We'll lose usage
    // data if the last time Chrome was run was more than a day ago.
    // Here, we prefer collecting less data but the collected data is
    // associated with an accurate date.
    if (days_since_last_update == 1) {
      // The previous day's data point is the second one from the tail.
      // Therefore (kNumDaysInHistory - 2) below.
      RecordDailyContentLengthHistograms(
          total.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          total.GetReceivedListPrefValue(kNumDaysInHistory - 2),
          proxy_enabled.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          proxy_enabled.GetReceivedListPrefValue(kNumDaysInHistory - 2),
          via_proxy.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          via_proxy.GetReceivedListPrefValue(kNumDaysInHistory - 2),
          https.GetListPrefValue(kNumDaysInHistory - 2),
          short_bypass.GetListPrefValue(kNumDaysInHistory - 2),
          long_bypass.GetListPrefValue(kNumDaysInHistory - 2),
          unknown.GetListPrefValue(kNumDaysInHistory - 2));
    }
  }
}

void UpdateContentLengthPrefs(
    int received_content_length,
    int original_content_length,
    bool with_data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    PrefService* prefs) {
  int64 total_received = prefs->GetInt64(
      data_reduction_proxy::prefs::kHttpReceivedContentLength);
  int64 total_original = prefs->GetInt64(
      data_reduction_proxy::prefs::kHttpOriginalContentLength);
  total_received += received_content_length;
  total_original += original_content_length;
  prefs->SetInt64(data_reduction_proxy::prefs::kHttpReceivedContentLength,
                  total_received);
  prefs->SetInt64(data_reduction_proxy::prefs::kHttpOriginalContentLength,
                  total_original);

  UpdateContentLengthPrefsForDataReductionProxy(
      received_content_length,
      original_content_length,
      with_data_reduction_proxy_enabled,
      request_type,
      base::Time::Now(),
      prefs);
}

}  // namespace data_reduction_proxy

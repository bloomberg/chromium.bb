// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_USAGE_STATS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_USAGE_STATS_H_

#include "base/callback.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/pref_member.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

namespace net {
class ProxyServer;
}

namespace data_reduction_proxy {

class DataReductionProxyUsageStats
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // Records a data reduction proxy bypass event as a "BlockType" if
  // |bypass_all| is true and as a "BypassType" otherwise. Records the event as
  // "Primary" if |is_primary| is true and "Fallback" otherwise.
  static void RecordDataReductionProxyBypassInfo(
      bool is_primary,
      bool bypass_all,
      const net::ProxyServer& proxy_server,
      DataReductionProxyBypassType bypass_type);

  // MessageLoopProxy instance is owned by io_thread. |params| outlives
  // this class instance.
  DataReductionProxyUsageStats(DataReductionProxyParams* params,
                               base::MessageLoopProxy* ui_thread_proxy);
  virtual ~DataReductionProxyUsageStats();

  // Sets the callback to be called on the UI thread when the unavailability
  // status has changed.
  void set_unavailable_callback(
      const base::Callback<void(bool)>& unavailable_callback) {
    unavailable_callback_ = unavailable_callback;
  }

  // Callback intended to be called from |ChromeNetworkDelegate| when a
  // request completes. This method is used to gather usage stats.
  void OnUrlRequestCompleted(const net::URLRequest* request, bool started);

  // Records the last bypass reason to |bypass_type_| and sets
  // |triggering_request_| to true. A triggering request is the first request to
  // cause the current bypass.
  void SetBypassType(DataReductionProxyBypassType type);

  // Given |data_reduction_proxy_enabled|, a |request|, and the
  // |data_reduction_proxy_config| records the number of bypassed bytes for that
  // |request| into UMAs based on bypass type. |data_reduction_proxy_enabled|
  // tells us the state of the kDataReductionProxyEnabled preference.
  void RecordBypassedBytesHistograms(
      net::URLRequest& request,
      const BooleanPrefMember& data_reduction_proxy_enabled,
      const net::ProxyConfig& data_reduction_proxy_config);

  void RecordBypassEventHistograms(const net::ProxyServer& bypassed_proxy,
                                   int net_error) const;

 private:
  enum BypassedBytesType {
    NOT_BYPASSED = 0,         /* Not bypassed. */
    SSL,                      /* Bypass due to SSL. */
    LOCAL_BYPASS_RULES,       /* Bypass due to client-side bypass rules. */
    MANAGED_PROXY_CONFIG,     /* Bypass due to managed config. */
    AUDIO_VIDEO,              /* Audio/Video bypass. */
    TRIGGERING_REQUEST,       /* Triggering request bypass. */
    NETWORK_ERROR,            /* Network error. */
    BYPASSED_BYTES_TYPE_MAX   /* This must always be last.*/
  };

  // NetworkChangeNotifier::NetworkChangeObserver:
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // Counts requests that went through the data reduction proxy and counts
  // requests that were eligible to go through the proxy.
  void IncrementRequestCounts(bool actual);
  void ClearRequestCounts();

  // Checks if the availability status of the data reduction proxy has changed,
  // and notifies the UIThread via NotifyUnavailabilityOnUIThread if so. The
  // data reduction proxy is considered unavailable if and only if no requests
  // went through the proxy but some eligible requests were service by other
  // routes.
  void MaybeNotifyUnavailability();
  void NotifyUnavailabilityOnUIThread(bool unavailable);

  DataReductionProxyParams* data_reduction_proxy_params_;
  // The last reason for bypass as determined by
  // MaybeBypassProxyAndPrepareToRetry
  DataReductionProxyBypassType last_bypass_type_;
  // True if the last request triggered the current bypass.
  bool triggering_request_;
  base::MessageLoopProxy* ui_thread_proxy_;

  // The following 2 fields are used to determine if data reduction proxy is
  // unreachable. We keep a count of requests which should go through
  // data request proxy, as well as those which actually do. The proxy is
  // unreachable if no successful requests are made through it despite a
  // non-zero number of requests being eligible.

  // Count of requests which will be tried to be sent through data reduction
  // proxy. The count is only based on the config and not the bad proxy list.
  // Explicit bypasses are not part of this count. This is the desired behavior
  // since otherwise both counts would be identical.
  unsigned long eligible_num_requests_through_proxy_;

  // Count of successful requests through data reduction proxy.
  unsigned long actual_num_requests_through_proxy_;

  // Whether or not the data reduction proxy is unavailable.
  bool unavailable_;

  base::ThreadChecker thread_checker_;

  void RecordBypassedBytes(
      DataReductionProxyBypassType bypass_type,
      BypassedBytesType bypassed_bytes_type,
      int64 content_length);

  // Called when the unavailability status has changed. Runs on the UI thread.
  base::Callback<void(bool)> unavailable_callback_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyUsageStats);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_USAGE_STATS_H_

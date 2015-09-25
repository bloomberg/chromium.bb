// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "net/base/layered_network_delegate.h"
#include "net/proxy/proxy_retry_info.h"

class GURL;

namespace net {
class HttpResponseHeaders;
class HttpRequestHeaders;
class NetLog;
class NetworkDelegate;
class ProxyConfig;
class ProxyInfo;
class ProxyServer;
class ProxyService;
class URLRequest;
}

namespace data_reduction_proxy {

class DataReductionProxyBypassStats;
class DataReductionProxyConfig;
class DataReductionProxyConfigurator;
class DataReductionProxyEventCreator;
class DataReductionProxyExperimentsStats;
class DataReductionProxyIOData;
class DataReductionProxyRequestOptions;

// DataReductionProxyNetworkDelegate is a LayeredNetworkDelegate that wraps a
// NetworkDelegate and adds Data Reduction Proxy specific logic.
class DataReductionProxyNetworkDelegate : public net::LayeredNetworkDelegate {
 public:
  // Provides an additional proxy configuration that can be consulted after
  // proxy resolution. Used to get the Data Reduction Proxy config and give it
  // to the OnResolveProxyHandler and RecordBytesHistograms.
  typedef base::Callback<const net::ProxyConfig&()> ProxyConfigGetter;

  // Constructs a DataReductionProxyNetworkdelegate object with the given
  // |network_delegate|, |config|, |handler|, |configurator|,
  // |experiments_stats|, |net_log|, and |event_creator|. Takes ownership of
  // and wraps the |network_delegate|, calling an internal implementation for
  // each delegate method. For example, the implementation of
  // OnHeadersReceived() calls OnHeadersReceivedInternal().
  DataReductionProxyNetworkDelegate(
      scoped_ptr<net::NetworkDelegate> network_delegate,
      DataReductionProxyConfig* config,
      DataReductionProxyRequestOptions* handler,
      const DataReductionProxyConfigurator* configurator,
      DataReductionProxyExperimentsStats* experiments_stats,
      net::NetLog* net_log,
      DataReductionProxyEventCreator* event_creator);
  ~DataReductionProxyNetworkDelegate() override;

  // Initializes member variables to record data reduction proxy prefs and
  // report UMA.
  void InitIODataAndUMA(
      DataReductionProxyIOData* io_data,
      DataReductionProxyBypassStats* bypass_stats);

  // Creates a |Value| summary of the state of the network session. The caller
  // is responsible for deleting the returned value.
  base::Value* SessionNetworkStatsInfoToValue() const;

 private:
  friend class DataReductionProxyNetworkDelegateTest;

  // Called as the proxy is being resolved for |url|. Allows the delegate to
  // override the proxy resolution decision made by ProxyService. The delegate
  // may override the decision by modifying the ProxyInfo |result|.
  void OnResolveProxyInternal(const GURL& url,
                              int load_flags,
                              const net::ProxyService& proxy_service,
                              net::ProxyInfo* result) override;

  // Called when use of |bad_proxy| fails due to |net_error|. |net_error| is
  // the network error encountered, if any, and OK if the fallback was
  // for a reason other than a network error (e.g. the proxy service was
  // explicitly directed to skip a proxy).
  void OnProxyFallbackInternal(const net::ProxyServer& bad_proxy,
                               int net_error) override;

  // Called after a proxy connection. Allows the delegate to read/write
  // |headers| before they get sent out. |headers| is valid only until
  // OnCompleted or OnURLRequestDestroyed is called for this request.
  void OnBeforeSendProxyHeadersInternal(
      net::URLRequest* request,
      const net::ProxyInfo& proxy_info,
      net::HttpRequestHeaders* headers) override;

  // Indicates that the URL request has been completed or failed.
  // |started| indicates whether the request has been started. If false,
  // some information like the socket address is not available.
  void OnCompletedInternal(net::URLRequest* request,
                           bool started) override;

  // Posts to the UI thread to UpdateContentLengthPrefs in the data reduction
  // proxy metrics and updates |received_content_length_| and
  // |original_content_length_|.
  void AccumulateDataUsage(int64 data_used,
                           int64 original_size,
                           DataReductionProxyRequestType request_type,
                           const std::string& data_usage_host,
                           const std::string& mime_type);

  // Total size of all content that has been received over the network.
  int64 total_received_bytes_;

  // Total original size of all content before it was transferred.
  int64 total_original_received_bytes_;

  // All raw Data Reduction Proxy pointers must outlive |this|.
  DataReductionProxyConfig* data_reduction_proxy_config_;

  DataReductionProxyBypassStats* data_reduction_proxy_bypass_stats_;

  DataReductionProxyRequestOptions* data_reduction_proxy_request_options_;

  DataReductionProxyIOData* data_reduction_proxy_io_data_;

  const DataReductionProxyConfigurator* configurator_;

  DataReductionProxyExperimentsStats* experiments_stats_;

  net::NetLog* net_log_;

  DataReductionProxyEventCreator* event_creator_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyNetworkDelegate);
};

// Adds data reduction proxies to |result|, where applicable, if result
// otherwise uses a direct connection for |url|, and the data reduction proxy is
// not bypassed. Also, configures |result| to proceed directly to the origin if
// |result|'s current proxy is the data reduction proxy, the
// |net::LOAD_BYPASS_DATA_REDUCTION_PROXY| |load_flag| is set, and the
// DataCompressionProxyCriticalBypass Finch trial is set.
void OnResolveProxyHandler(const GURL& url,
                           int load_flags,
                           const net::ProxyConfig& data_reduction_proxy_config,
                           const net::ProxyRetryInfoMap& proxy_retry_info,
                           const DataReductionProxyConfig* config,
                           net::ProxyInfo* result);

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_

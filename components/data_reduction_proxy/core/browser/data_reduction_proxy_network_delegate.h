// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "net/base/layered_network_delegate.h"
#include "net/proxy/proxy_retry_info.h"

class GURL;

namespace base {
class Value;
}

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
class DataReductionProxyIOData;
class DataReductionProxyRequestOptions;

// Values of the UMA DataReductionProxy.LoFi.TransformationType histogram.
// This enum must remain synchronized with
// DataReductionProxyLoFiTransformationType in
// metrics/histograms/histograms.xml.
enum LoFiTransformationType {
  PREVIEW = 0,
  NO_TRANSFORMATION_PREVIEW_REQUESTED,
  LO_FI_TRANSFORMATION_TYPES_INDEX_BOUNDARY,
};

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
  // |net_log|, and |event_creator|. Takes ownership of and wraps the
  // |network_delegate|, calling an internal implementation for each delegate
  // method. For example, the implementation of OnHeadersReceived() calls
  // OnHeadersReceivedInternal().
  DataReductionProxyNetworkDelegate(
      scoped_ptr<net::NetworkDelegate> network_delegate,
      DataReductionProxyConfig* config,
      DataReductionProxyRequestOptions* handler,
      const DataReductionProxyConfigurator* configurator,
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

  // Calculates actual data usage that went over the network at the HTTP layer
  // (e.g. not including network layer overhead) and estimates original data
  // usage for |request|. Passing in -1 for |original_content_length| indicates
  // that the original content length of the response could not be determined.
  void CalculateAndRecordDataUsage(const net::URLRequest& request,
                                   DataReductionProxyRequestType request_type,
                                   int64_t original_content_length);

  // Posts to the UI thread to UpdateContentLengthPrefs in the data reduction
  // proxy metrics and updates |received_content_length_| and
  // |original_content_length_|.
  void AccumulateDataUsage(int64_t data_used,
                           int64_t original_size,
                           DataReductionProxyRequestType request_type,
                           const std::string& data_usage_host,
                           const std::string& mime_type);

  // Record information such as histograms related to the Content-Length of
  // |request|. |original_content_length| is the length of the resource if
  // fetched over a direct connection without the Data Reduction Proxy, or -1 if
  // no original content length is available.
  void RecordContentLength(const net::URLRequest& request,
                           DataReductionProxyRequestType request_type,
                           int64_t original_content_length);

  // Records UMA that counts how many pages were transformed by various Lo-Fi
  // transformations.
  void RecordLoFiTransformationType(LoFiTransformationType type);

  // Total size of all content that has been received over the network.
  int64_t total_received_bytes_;

  // Total original size of all content before it was transferred.
  int64_t total_original_received_bytes_;

  // All raw Data Reduction Proxy pointers must outlive |this|.
  DataReductionProxyConfig* data_reduction_proxy_config_;

  DataReductionProxyBypassStats* data_reduction_proxy_bypass_stats_;

  DataReductionProxyRequestOptions* data_reduction_proxy_request_options_;

  DataReductionProxyIOData* data_reduction_proxy_io_data_;

  const DataReductionProxyConfigurator* configurator_;

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

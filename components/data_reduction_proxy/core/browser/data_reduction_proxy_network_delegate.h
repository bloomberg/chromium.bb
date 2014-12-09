// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "net/base/layered_network_delegate.h"
#include "net/proxy/proxy_retry_info.h"

template<class T> class PrefMember;

typedef PrefMember<bool> BooleanPrefMember;

class GURL;
class PrefService;

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class HttpResponseHeaders;
class HttpRequestHeaders;
class NetworkDelegate;
class ProxyConfig;
class ProxyInfo;
class ProxyServer;
class ProxyService;
class URLRequest;
}

namespace data_reduction_proxy {

class DataReductionProxyAuthRequestHandler;
class DataReductionProxyParams;
class DataReductionProxyStatisticsPrefs;
class DataReductionProxyUsageStats;

// DataReductionProxyNetworkDelegate is a LayeredNetworkDelegate that wraps a
// NetworkDelegate and adds Data Reduction Proxy specific logic.
class DataReductionProxyNetworkDelegate : public net::LayeredNetworkDelegate {
 public:
  // Provides an opportunity to interpose on proxy resolution. Called before
  // ProxyService.ResolveProxy() returns. Two proxy configurations are provided
  // that specify the data reduction proxy's configuration and the effective
  // configuration according to the proxy service, respectively. Retry info is
  // presumed to be from the proxy service.
  typedef base::Callback<void(
      const GURL& url,
      int load_flags,
      const net::ProxyConfig& data_reduction_proxy_config,
      const net::ProxyConfig& proxy_service_proxy_config,
      const net::ProxyRetryInfoMap& proxy_retry_info_map,
      const DataReductionProxyParams* params,
      net::ProxyInfo* result)> OnResolveProxyHandler;

  // Provides an additional proxy configuration that can be consulted after
  // proxy resolution. Used to get the Data Reduction Proxy config and give it
  // to the OnResolveProxyHandler and RecordBytesHistograms.
  typedef base::Callback<const net::ProxyConfig&()> ProxyConfigGetter;

  // Constructs a DataReductionProxyNetworkdelegate object with the given
  // |network_delegate|, |params|, |handler|, and |getter|. Takes ownership of
  // and wraps the |network_delegate|, calling an internal implementation for
  // each delegate method. For example, the implementation of
  // OnHeadersReceived() calls OnHeadersReceivedInternal().
  DataReductionProxyNetworkDelegate(
      scoped_ptr<net::NetworkDelegate> network_delegate,
      DataReductionProxyParams* params,
      DataReductionProxyAuthRequestHandler* handler,
      const ProxyConfigGetter& getter);
  ~DataReductionProxyNetworkDelegate() override;

  // Initializes member variables used for overriding the proxy config.
  // |proxy_config_getter_| must be non-NULL when this is called.
  void InitProxyConfigOverrider(const OnResolveProxyHandler& proxy_handler);

  // Initializes member variables to record data reduction proxy prefs and
  // report UMA.
  void InitStatisticsPrefsAndUMA(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      DataReductionProxyStatisticsPrefs* statistics_prefs,
      BooleanPrefMember* data_reduction_proxy_enabled,
      DataReductionProxyUsageStats* usage_stats);

  // Creates a |Value| summary of the persistent state of the network session.
  // The caller is responsible for deleting the returned value.
  // Must be called on the UI thread.
  static base::Value* HistoricNetworkStatsInfoToValue(
      PrefService* profile_prefs);

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

  // Posts to the UI thread to UpdateContentLengthPrefs in the data reduction
  // proxy metrics and updates |received_content_length_| and
  // |original_content_length_|.
  void AccumulateContentLength(int64 received_content_length,
                               int64 original_content_length,
                               DataReductionProxyRequestType request_type);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Total size of all content (excluding headers) that has been received
  // over the network.
  int64 received_content_length_;

  // Total original size of all content before it was transferred.
  int64 original_content_length_;

  // Weak, owned by our owner.
  BooleanPrefMember* data_reduction_proxy_enabled_;

  // Must outlive this DataReductionProxyNetworkDelegate.
  DataReductionProxyParams* data_reduction_proxy_params_;

  // Must outlive this DataReductionProxyNetworkDelegate.
  DataReductionProxyUsageStats* data_reduction_proxy_usage_stats_;

  DataReductionProxyAuthRequestHandler*
      data_reduction_proxy_auth_request_handler_;

  DataReductionProxyStatisticsPrefs* data_reduction_proxy_statistics_prefs_;

  OnResolveProxyHandler on_resolve_proxy_handler_;

  ProxyConfigGetter proxy_config_getter_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyNetworkDelegate);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_

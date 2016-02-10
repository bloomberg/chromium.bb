// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_

#include <stdint.h>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_debug_ui_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "components/data_reduction_proxy/core/common/lofi_ui_service.h"

namespace base {
class Value;
}

namespace net {
class NetLog;
class URLRequestContextGetter;
class URLRequestInterceptor;
}

namespace data_reduction_proxy {

class DataReductionProxyBypassStats;
class DataReductionProxyConfig;
class DataReductionProxyConfigServiceClient;
class DataReductionProxyConfigurator;
class DataReductionProxyEventCreator;
class DataReductionProxyService;

// Contains and initializes all Data Reduction Proxy objects that operate on
// the IO thread.
class DataReductionProxyIOData : public DataReductionProxyEventStorageDelegate {
 public:
  // Constructs a DataReductionProxyIOData object. |param_flags| is used to
  // set information about the DNS names used by the proxy, and allowable
  // configurations. |enabled| sets the initial state of the Data Reduction
  // Proxy.
  DataReductionProxyIOData(
      const Client& client,
      int param_flags,
      net::NetLog* net_log,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      bool enabled,
      bool enable_quic,
      const std::string& user_agent);

  virtual ~DataReductionProxyIOData();

  // Performs UI thread specific shutdown logic.
  void ShutdownOnUIThread();

  // Sets the Data Reduction Proxy service after it has been created.
  // Virtual for testing.
  virtual void SetDataReductionProxyService(
      base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service);

  // Creates an interceptor suitable for following the Data Reduction Proxy
  // bypass protocol.
  scoped_ptr<net::URLRequestInterceptor> CreateInterceptor();

  // Creates a NetworkDelegate suitable for carrying out the Data Reduction
  // Proxy protocol, including authenticating, establishing a handler to
  // override the current proxy configuration, and
  // gathering statistics for UMA.
  scoped_ptr<DataReductionProxyNetworkDelegate> CreateNetworkDelegate(
      scoped_ptr<net::NetworkDelegate> wrapped_network_delegate,
      bool track_proxy_bypass_statistics);

  // Sets user defined preferences for how the Data Reduction Proxy
  // configuration should be set. |at_startup| is true only
  // when DataReductionProxySettings is initialized.
  void SetProxyPrefs(bool enabled, bool at_startup);

  // Applies a serialized Data Reduction Proxy configuration.
  void SetDataReductionProxyConfiguration(const std::string& serialized_config);

  // Returns true when Lo-Fi mode should be activated. When Lo-Fi mode is
  // active, URL requests are modified to request low fidelity versions of the
  // resources, except when the user is in the Lo-Fi control group.
  bool ShouldEnableLoFiMode(const net::URLRequest& request);

  // Sets Lo-Fi mode off in |config_|.
  void SetLoFiModeOff();

  // Bridge methods to safely call to the UI thread objects.
  void UpdateContentLengths(int64_t data_used,
                            int64_t original_size,
                            bool data_reduction_proxy_enabled,
                            DataReductionProxyRequestType request_type,
                            const std::string& data_usage_host,
                            const std::string& mime_type);
  void SetLoFiModeActiveOnMainFrame(bool lo_fi_mode_active);

  // Overrides of DataReductionProxyEventStorageDelegate. Bridges to the UI
  // thread objects.
  void AddEvent(scoped_ptr<base::Value> event) override;
  void AddEnabledEvent(scoped_ptr<base::Value> event, bool enabled) override;
  void AddEventAndSecureProxyCheckState(scoped_ptr<base::Value> event,
                                        SecureProxyCheckState state) override;
  void AddAndSetLastBypassEvent(scoped_ptr<base::Value> event,
                                int64_t expiration_ticks) override;

  // Returns true if the Data Reduction Proxy is enabled and false otherwise.
  bool IsEnabled() const;

  // Various accessor methods.
  DataReductionProxyConfigurator* configurator() const {
    return configurator_.get();
  }

  DataReductionProxyConfig* config() const {
    return config_.get();
  }

  DataReductionProxyEventCreator* event_creator() const {
    return event_creator_.get();
  }

  DataReductionProxyRequestOptions* request_options() const {
    return request_options_.get();
  }

  DataReductionProxyConfigServiceClient* config_client() const {
    return config_client_.get();
  }

  net::ProxyDelegate* proxy_delegate() const {
    return proxy_delegate_.get();
  }

  net::NetLog* net_log() {
    return net_log_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner() const {
    return io_task_runner_;
  }

  // Used for testing.
  DataReductionProxyBypassStats* bypass_stats() const {
    return bypass_stats_.get();
  }

  DataReductionProxyDebugUIService* debug_ui_service() const {
    return debug_ui_service_.get();
  }

  void set_debug_ui_service(
      scoped_ptr<DataReductionProxyDebugUIService> ui_service) const {
    debug_ui_service_ = std::move(ui_service);
  }

  LoFiDecider* lofi_decider() const { return lofi_decider_.get(); }

  void set_lofi_decider(scoped_ptr<LoFiDecider> lofi_decider) const {
    lofi_decider_ = std::move(lofi_decider);
  }

  LoFiUIService* lofi_ui_service() const { return lofi_ui_service_.get(); }

  // Takes ownership of |lofi_ui_service|.
  void set_lofi_ui_service(scoped_ptr<LoFiUIService> lofi_ui_service) const {
    lofi_ui_service_ = std::move(lofi_ui_service);
  }

 private:
  friend class TestDataReductionProxyIOData;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyIODataTest, TestConstruction);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyIODataTest,
                           TestResetBadProxyListOnDisableDataSaver);

  // Used for testing.
  DataReductionProxyIOData();

  // Initializes the weak pointer to |this| on the IO thread. It must be done
  // on the IO thread, since it is used for posting tasks from the UI thread
  // to IO thread objects in a thread safe manner.
  void InitializeOnIOThread();

  // Records that the data reduction proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Stores an int64_t value in preferences storage.
  void SetInt64Pref(const std::string& pref_path, int64_t value);

  // Stores a string value in preferences storage.
  void SetStringPref(const std::string& pref_path, const std::string& value);

  // Stores a serialized Data Reduction Proxy configuration in preferences
  // storage.
  void StoreSerializedConfig(const std::string& serialized_config);

  // The type of Data Reduction Proxy client.
  Client client_;

  // Parameters including DNS names and allowable configurations.
  scoped_ptr<DataReductionProxyConfig> config_;

  // Holds the DataReductionProxyDebugUIManager for Data Reduction Proxy bypass
  // interstitials.
  mutable scoped_ptr<DataReductionProxyDebugUIService> debug_ui_service_;

  // Handles getting if a request is in Lo-Fi mode.
  mutable scoped_ptr<LoFiDecider> lofi_decider_;

  // Handles showing Lo-Fi UI when a Lo-Fi response is received.
  mutable scoped_ptr<LoFiUIService> lofi_ui_service_;

  // Creates Data Reduction Proxy-related events for logging.
  scoped_ptr<DataReductionProxyEventCreator> event_creator_;

  // Setter of the Data Reduction Proxy-specific proxy configuration.
  scoped_ptr<DataReductionProxyConfigurator> configurator_;

  // A proxy delegate. Used, for example, to add authentication to HTTP CONNECT
  // request.
  scoped_ptr<DataReductionProxyDelegate> proxy_delegate_;

  // Data Reduction Proxy objects with a UI based lifetime.
  base::WeakPtr<DataReductionProxyService> service_;

  // Tracker of various metrics to be reported in UMA.
  scoped_ptr<DataReductionProxyBypassStats> bypass_stats_;

  // Constructs credentials suitable for authenticating the client.
  scoped_ptr<DataReductionProxyRequestOptions> request_options_;

  // Requests new Data Reduction Proxy configurations from a remote service.
  scoped_ptr<DataReductionProxyConfigServiceClient> config_client_;

  // A net log.
  net::NetLog* net_log_;

  // IO and UI task runners, respectively.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Whether the Data Reduction Proxy has been enabled or not by the user. In
  // practice, this can be overridden by the command line.
  bool enabled_;

  // The net::URLRequestContextGetter used for making URL requests.
  net::URLRequestContextGetter* url_request_context_getter_;

  // A net::URLRequestContextGetter used for making secure proxy checks. It
  // does not use alternate protocols.
  scoped_refptr<net::URLRequestContextGetter> basic_url_request_context_getter_;

  base::WeakPtrFactory<DataReductionProxyIOData> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyIOData);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_

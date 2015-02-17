// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_DATA_REDUCTION_PROXY_DEBUG_UI_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_DATA_REDUCTION_PROXY_DEBUG_UI_SERVICE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_debug_ui_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class ProxyConfig;
}

namespace data_reduction_proxy {

class DataReductionProxyDebugUIManager;

// Creates a DataReductionProxyDebugUIManager which handles showing
// interstitials. Also holds a ProxyConfigGetter which the
// DataReductionProxyDebugResourceThrottle uses to decide when to display the
// interstitials.
class ContentDataReductionProxyDebugUIService
    : public DataReductionProxyDebugUIService {
 public:
  // Constructs a ContentDataReductionProxyDebugUIService object. |getter| is
  // used to get the Data Reduction Proxy config. |ui_task_runner| and
  // |io_task_runner| are used to create a DataReductionDebugProxyUIService.
  ContentDataReductionProxyDebugUIService(
      const DataReductionProxyNetworkDelegate::ProxyConfigGetter& getter,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const std::string& app_locale);

  ~ContentDataReductionProxyDebugUIService() override;

  const net::ProxyConfig& data_reduction_proxy_config() const override;

  const scoped_refptr<DataReductionProxyDebugUIManager>&
      ui_manager() const override;

 private:
  // The UI manager handles showing interstitials. Accessed on both UI and IO
  // thread.
  scoped_refptr<DataReductionProxyDebugUIManager> ui_manager_;
  DataReductionProxyNetworkDelegate::ProxyConfigGetter proxy_config_getter_;

  DISALLOW_COPY_AND_ASSIGN(ContentDataReductionProxyDebugUIService);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_DATA_REDUCTION_PROXY_DEBUG_UI_SERVICE_H_

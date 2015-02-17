// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_data_reduction_proxy_debug_ui_service.h"

#include <string>

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager.h"
#include "net/proxy/proxy_config.h"

namespace data_reduction_proxy {

ContentDataReductionProxyDebugUIService::
ContentDataReductionProxyDebugUIService(
    const DataReductionProxyNetworkDelegate::ProxyConfigGetter& getter,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const std::string& app_locale)
    : proxy_config_getter_(getter) {
  ui_manager_ = new DataReductionProxyDebugUIManager(ui_task_runner,
                                                     io_task_runner,
                                                     app_locale);
}

ContentDataReductionProxyDebugUIService::
~ContentDataReductionProxyDebugUIService() {
}

const net::ProxyConfig&
ContentDataReductionProxyDebugUIService::data_reduction_proxy_config() const {
  return proxy_config_getter_.Run();
}

const scoped_refptr<DataReductionProxyDebugUIManager>&
ContentDataReductionProxyDebugUIService::ui_manager() const {
  return ui_manager_;
}

}  // namespace data_reduction_proxy

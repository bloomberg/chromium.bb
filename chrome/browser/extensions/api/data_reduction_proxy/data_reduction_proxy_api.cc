// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/data_reduction_proxy/data_reduction_proxy_api.h"

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

namespace extensions {

AsyncExtensionFunction::ResponseAction
DataReductionProxyClearDataSavingsFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  data_reduction_proxy::DataReductionProxySettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context());
  settings->data_reduction_proxy_service()->compression_stats()->
      ClearDataSavingStatistics();
  return RespondNow(NoArguments());
}

}  // namespace extensions

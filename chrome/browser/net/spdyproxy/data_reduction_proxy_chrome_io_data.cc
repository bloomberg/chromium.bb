// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_io_data.h"

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(ENABLE_DATA_REDUCTION_PROXY_DEBUGGING)
#include "chrome/browser/browser_process.h"
#include "components/data_reduction_proxy/content/browser/content_data_reduction_proxy_debug_ui_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#endif

namespace content {
class BrowserContext;
}

using data_reduction_proxy::DataReductionProxyParams;

scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
CreateDataReductionProxyChromeIOData(
    net::NetLog* net_log,
    PrefService* prefs,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    bool enable_quic) {
  DCHECK(net_log);
  DCHECK(prefs);

  int flags = DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kAlternativeAllowed;
  if (DataReductionProxyParams::IsIncludedInPromoFieldTrial())
    flags |= DataReductionProxyParams::kPromoAllowed;
  if (DataReductionProxyParams::IsIncludedInHoldbackFieldTrial())
    flags |= DataReductionProxyParams::kHoldback;
#if defined(OS_ANDROID)
  if (DataReductionProxyParams::IsIncludedInAndroidOnePromoFieldTrial(
          base::android::BuildInfo::GetInstance()->android_build_fp())) {
    flags |= DataReductionProxyParams::kPromoAllowed;
  }
#endif

  scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data(
          new data_reduction_proxy::DataReductionProxyIOData(
              DataReductionProxyChromeSettings::GetClient(), flags, net_log,
              io_task_runner, ui_task_runner, enable_quic));
  data_reduction_proxy_io_data->InitOnUIThread(prefs);

#if defined(ENABLE_DATA_REDUCTION_PROXY_DEBUGGING)
  scoped_ptr<data_reduction_proxy::ContentDataReductionProxyDebugUIService>
      data_reduction_proxy_ui_service(
          new data_reduction_proxy::ContentDataReductionProxyDebugUIService(
              base::Bind(&data_reduction_proxy::DataReductionProxyConfigurator::
                             GetProxyConfigOnIOThread,
                         base::Unretained(
                             data_reduction_proxy_io_data->configurator())),
              ui_task_runner, io_task_runner,
              g_browser_process->GetApplicationLocale()));
  data_reduction_proxy_io_data->set_debug_ui_service(
      data_reduction_proxy_ui_service.Pass());
#endif

  return data_reduction_proxy_io_data.Pass();
}

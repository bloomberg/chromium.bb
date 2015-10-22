// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_io_data.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/chrome_content_client.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_retrieval_params.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_experiments_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"

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
              DataReductionProxyParams::kFallbackAllowed;
  if (data_reduction_proxy::params::IsIncludedInPromoFieldTrial())
    flags |= DataReductionProxyParams::kPromoAllowed;
  if (data_reduction_proxy::params::IsIncludedInHoldbackFieldTrial())
    flags |= DataReductionProxyParams::kHoldback;
#if defined(OS_ANDROID)
  if (data_reduction_proxy::params::IsIncludedInAndroidOnePromoFieldTrial(
          base::android::BuildInfo::GetInstance()->android_build_fp())) {
    flags |= DataReductionProxyParams::kPromoAllowed;
  }
#endif

  bool enabled =
      prefs->GetBoolean(
          data_reduction_proxy::prefs::kDataReductionProxyEnabled) ||
      data_reduction_proxy::params::ShouldForceEnableDataReductionProxy();
  scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data(
          new data_reduction_proxy::DataReductionProxyIOData(
              DataReductionProxyChromeSettings::GetClient(), flags, net_log,
              io_task_runner, ui_task_runner, enabled, enable_quic,
              GetUserAgent()));
  data_reduction_proxy_io_data->experiments_stats()->InitializeOnUIThread(
      data_reduction_proxy::DataReductionProxyConfigRetrievalParams::Create(
          prefs));

  data_reduction_proxy_io_data->set_lofi_decider(
      make_scoped_ptr(new data_reduction_proxy::ContentLoFiDecider()));

#if defined(ENABLE_DATA_REDUCTION_PROXY_DEBUGGING)
  scoped_ptr<data_reduction_proxy::ContentDataReductionProxyDebugUIService>
      data_reduction_proxy_ui_service(
          new data_reduction_proxy::ContentDataReductionProxyDebugUIService(
              base::Bind(&data_reduction_proxy::DataReductionProxyConfigurator::
                             GetProxyConfig,
                         base::Unretained(
                             data_reduction_proxy_io_data->configurator())),
              ui_task_runner, io_task_runner,
              g_browser_process->GetApplicationLocale()));
  data_reduction_proxy_io_data->set_debug_ui_service(
      data_reduction_proxy_ui_service.Pass());
#endif

  return data_reduction_proxy_io_data.Pass();
}

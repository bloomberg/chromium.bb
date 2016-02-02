// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_io_data.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_ui_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_retrieval_params.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_experiments_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/android/tab_android.h"
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

namespace {

// For Android builds, notifies the TabAndroid associated with |web_contents|
// that a Lo-Fi response has been received. The TabAndroid then handles showing
// Lo-Fi UI if this is the first Lo-Fi response for a page load. |is_preview|
// indicates whether the response was a Lo-Fi preview response.
void OnLoFiResponseReceivedOnUI(content::WebContents* web_contents,
                                bool is_preview) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(OS_ANDROID)
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (tab)
    tab->OnLoFiResponseReceived(is_preview);
#endif
}

} // namespace

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
      prefs->GetBoolean(prefs::kDataSaverEnabled) ||
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
  data_reduction_proxy_io_data->set_lofi_ui_service(
      make_scoped_ptr(new data_reduction_proxy::ContentLoFiUIService(
          ui_task_runner,
          base::Bind(&OnLoFiResponseReceivedOnUI))));

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
      std::move(data_reduction_proxy_ui_service));
#endif

  return data_reduction_proxy_io_data;
}

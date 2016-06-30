// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_io_data.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_ui_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/android/tab_android.h"
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

std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
CreateDataReductionProxyChromeIOData(
    net::NetLog* net_log,
    PrefService* prefs,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
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
  std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data(
          new data_reduction_proxy::DataReductionProxyIOData(
              DataReductionProxyChromeSettings::GetClient(), flags, net_log,
              io_task_runner, ui_task_runner, enabled, GetUserAgent(),
              version_info::GetChannelString(chrome::GetChannel())));

  data_reduction_proxy_io_data->set_lofi_decider(
      base::WrapUnique(new data_reduction_proxy::ContentLoFiDecider()));
  data_reduction_proxy_io_data->set_lofi_ui_service(
      base::WrapUnique(new data_reduction_proxy::ContentLoFiUIService(
          ui_task_runner, base::Bind(&OnLoFiResponseReceivedOnUI))));

  return data_reduction_proxy_io_data;
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_io_data.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/net/spdyproxy/chrome_data_use_group_provider.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/previews/previews_infobar_delegate.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_ui_service.h"
#include "components/data_reduction_proxy/content/browser/content_resource_type_provider.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {
class BrowserContext;
}

using data_reduction_proxy::DataReductionProxyParams;

namespace {

// If this is the first Lo-Fi response for a page load, a
// PreviewsInfoBarDelegate is created, which handles showing Lo-Fi UI.
void OnLoFiResponseReceivedOnUI(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PreviewsInfoBarDelegate::Create(
      web_contents, PreviewsInfoBarDelegate::LOFI,
      true /* is_data_saver_user */,
      PreviewsInfoBarDelegate::OnDismissPreviewsInfobarCallback());
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

  int flags = 0;
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
      base::MakeUnique<data_reduction_proxy::ContentLoFiDecider>());
  data_reduction_proxy_io_data->set_resource_type_provider(
      base::MakeUnique<data_reduction_proxy::ContentResourceTypeProvider>());
  data_reduction_proxy_io_data->set_lofi_ui_service(
      base::MakeUnique<data_reduction_proxy::ContentLoFiUIService>(
          ui_task_runner, base::Bind(&OnLoFiResponseReceivedOnUI)));
  data_reduction_proxy_io_data->set_data_usage_source_provider(
      base::MakeUnique<ChromeDataUseGroupProvider>());

  return data_reduction_proxy_io_data;
}

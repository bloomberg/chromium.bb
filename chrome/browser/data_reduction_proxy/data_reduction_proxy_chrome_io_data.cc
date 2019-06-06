// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_io_data.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
CreateDataReductionProxyChromeIOData(
    Profile* profile,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  DCHECK(profile->GetPrefs());

  bool enabled = data_reduction_proxy::DataReductionProxySettings::
      IsDataSaverEnabledByUser(profile->GetPrefs());

  std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data(
          new data_reduction_proxy::DataReductionProxyIOData(
              DataReductionProxyChromeSettings::GetClient(),
              profile->GetPrefs(), content::GetNetworkConnectionTracker(),
              io_task_runner, ui_task_runner, enabled, GetUserAgent(),
              version_info::GetChannelString(chrome::GetChannel())));

  return data_reduction_proxy_io_data;
}

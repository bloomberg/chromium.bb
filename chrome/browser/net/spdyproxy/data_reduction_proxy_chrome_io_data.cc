// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_io_data.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

namespace content {
class BrowserContext;
}

scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
CreateDataReductionProxyChromeIOData(
    net::NetLog* net_log,
    content::BrowserContext* browser_context,
    PrefService* prefs,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  DCHECK(net_log);
  DCHECK(prefs);
  DCHECK(browser_context);
  DataReductionProxyChromeSettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);

#if defined(OS_ANDROID) || defined(OS_IOS)
  // On mobile we write data reduction proxy prefs directly to the pref service.
  // On desktop we store data reduction proxy prefs in memory, writing to disk
  // every 60 minutes and on termination. Shutdown hooks must be added for
  // Android and iOS in order for non-zero delays to be supported.
  // (http://crbug.com/408264)
  base::TimeDelta commit_delay = base::TimeDelta();
#else
  base::TimeDelta commit_delay = base::TimeDelta::FromMinutes(60);
#endif

  data_reduction_proxy::DataReductionProxyStatisticsPrefs*
      data_reduction_proxy_statistics_prefs =
          new data_reduction_proxy::DataReductionProxyStatisticsPrefs(
              prefs, ui_task_runner,
              commit_delay);

  scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data(
          new data_reduction_proxy::DataReductionProxyIOData(
              DataReductionProxyChromeSettings::GetClient(),
              make_scoped_ptr(data_reduction_proxy_statistics_prefs),
              settings,
              net_log,
              io_task_runner,
              ui_task_runner));
  data_reduction_proxy_io_data->InitOnUIThread(prefs);
  settings->SetDataReductionProxyStatisticsPrefs(
      data_reduction_proxy_statistics_prefs);

  return data_reduction_proxy_io_data.Pass();
}

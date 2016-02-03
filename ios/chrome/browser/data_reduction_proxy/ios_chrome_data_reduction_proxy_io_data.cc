// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/data_reduction_proxy/ios_chrome_data_reduction_proxy_io_data.h"

#include "base/prefs/pref_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_retrieval_params.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_experiments_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "ios/chrome/browser/data_reduction_proxy/ios_chrome_data_reduction_proxy_settings.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/web/public/web_client.h"

using data_reduction_proxy::DataReductionProxyParams;

scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
CreateIOSChromeDataReductionProxyIOData(
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

  bool enabled =
      prefs->GetBoolean(prefs::kDataSaverEnabled) ||
      data_reduction_proxy::params::ShouldForceEnableDataReductionProxy();
  scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data(
          new data_reduction_proxy::DataReductionProxyIOData(
              IOSChromeDataReductionProxySettings::GetClient(), flags, net_log,
              io_task_runner, ui_task_runner, enabled, enable_quic,
              web::GetWebClient()->GetUserAgent(false)));
  data_reduction_proxy_io_data->experiments_stats()->InitializeOnUIThread(
      data_reduction_proxy::DataReductionProxyConfigRetrievalParams::Create(
          prefs));

  return data_reduction_proxy_io_data;
}

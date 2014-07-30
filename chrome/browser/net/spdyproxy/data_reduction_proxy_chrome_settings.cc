// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_configurator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"

using data_reduction_proxy::DataReductionProxyParams;
using data_reduction_proxy::DataReductionProxySettings;

DataReductionProxyChromeSettings::DataReductionProxyChromeSettings(
    DataReductionProxyParams* params) : DataReductionProxySettings(params) {
}

DataReductionProxyChromeSettings::~DataReductionProxyChromeSettings() {
}

void DataReductionProxyChromeSettings::InitDataReductionProxySettings(
    Profile* profile) {
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();

  scoped_ptr<data_reduction_proxy::DataReductionProxyConfigurator>
      configurator(new DataReductionProxyChromeConfigurator(prefs));
  SetProxyConfigurator(configurator.Pass());
  DataReductionProxySettings::InitDataReductionProxySettings(
      prefs,
      g_browser_process->local_state(),
      ProfileManager::GetActiveUserProfile()->GetRequestContext());

  SetDataReductionProxyAlternativeEnabled(
      DataReductionProxyParams::IsIncludedInAlternativeFieldTrial());
}

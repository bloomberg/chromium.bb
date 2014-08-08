// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_

#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class PrefService;
}

namespace data_reduction_proxy {
class DataReductionProxyConfigurator;
class DataReductionProxyParams;
}

namespace net {
class URLRequestContextGetter;
}

class PrefService;

// Data reduction proxy settings class suitable for use with a Chrome browser.
// It is keyed to a browser context.
class DataReductionProxyChromeSettings
    : public data_reduction_proxy::DataReductionProxySettings,
      public KeyedService {
 public:
  // Constructs a settings object with the given configuration parameters.
  // Construction and destruction must happen on the UI thread.
  explicit DataReductionProxyChromeSettings(
      data_reduction_proxy::DataReductionProxyParams* params);

  // Destructs the settings object.
  virtual ~DataReductionProxyChromeSettings();

  // Initialize the settings object with the given configurator, prefs services,
  // and request context.
  void InitDataReductionProxySettings(
      scoped_ptr<data_reduction_proxy::DataReductionProxyConfigurator>
          configurator,
      PrefService* profile_prefs,
      PrefService* local_state_prefs,
      net::URLRequestContextGetter* request_context);

  // Using the chrome::VersionInfo version, returns the build and patch portion
  // of the string delimited by a period. If the chrome::VersionInfo version
  // isn't of the form xx.xx.xx.xx returns an empty string.
  static std::string GetBuildAndPatchNumber();

  // Gets the client type for the data reduction proxy.
  static std::string GetClient();

 private:
  // Registers the DataReductionProxyEnabled synthetic field trial with
  // the group |data_reduction_proxy_enabled|.
  void RegisterSyntheticFieldTrial(bool data_reduction_proxy_enabled);

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyChromeSettings);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_

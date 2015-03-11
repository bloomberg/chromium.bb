// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace base {
class SingleThreadTaskRunner;
}

namespace data_reduction_proxy {
class DataReductionProxyIOData;
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
  // Constructs a settings object. Construction and destruction must happen on
  // the UI thread.
  DataReductionProxyChromeSettings();

  // Destructs the settings object.
  ~DataReductionProxyChromeSettings() override;

  // Overrides KeyedService::Shutdown:
  void Shutdown() override;

  // Initialize the settings object with the given io_data, prefs services,
  // request context getter, and task runner.
  void InitDataReductionProxySettings(
      data_reduction_proxy::DataReductionProxyIOData* io_data,
      PrefService* profile_prefs,
      net::URLRequestContextGetter* request_context_getter,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

  // Gets the client type for the data reduction proxy.
  static data_reduction_proxy::Client GetClient();

  // Public for testing.
  void MigrateDataReductionProxyOffProxyPrefs(PrefService* prefs);

 private:
  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyChromeSettings);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_

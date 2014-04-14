// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_IOS_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_IOS_H_

#include "components/data_reduction_proxy/content/data_reduction_proxy_settings.h"

// Central point for configuring the data reduction proxy on iOS.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
class DataReductionProxySettingsIOS
    : data_reduction_proxy::DataReductionProxySettings {
 public:
  DataReductionProxySettingsIOS();
  virtual ~DataReductionProxySettingsIOS() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettingsIOS);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_IOS_H_

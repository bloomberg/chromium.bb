// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GATEWAY_DATA_PROVIDER_LINUX_H_
#define CONTENT_BROWSER_GEOLOCATION_GATEWAY_DATA_PROVIDER_LINUX_H_
#pragma once

#include "content/browser/geolocation/gateway_data_provider_common.h"

class GatewayDataProviderLinux : public GatewayDataProviderCommon {
 public:
  GatewayDataProviderLinux();

 private:
  virtual ~GatewayDataProviderLinux();

  // GatewayDataProviderCommon
  virtual GatewayApiInterface* NewGatewayApi();
  DISALLOW_COPY_AND_ASSIGN(GatewayDataProviderLinux);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_GATEWAY_DATA_PROVIDER_LINUX_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_ANDROID_CHROME_DATA_REDUCTION_PROXY_ANDROID_H_
#define CHROME_APP_ANDROID_CHROME_DATA_REDUCTION_PROXY_ANDROID_H_

#include "net/http/http_network_session.h"

// Sets up state to interact with the data reduction proxy.
class ChromeDataReductionProxyAndroid {
 public:
  static void Init(net::HttpNetworkSession* session);

 protected:
  ChromeDataReductionProxyAndroid();
  virtual ~ChromeDataReductionProxyAndroid();


  DISALLOW_COPY_AND_ASSIGN(ChromeDataReductionProxyAndroid);
};

#endif  // CHROME_APP_ANDROID_CHROME_DATA_REDUCTION_PROXY_ANDROID_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_CHROME_URL_FIXER_CLIENT_H_
#define CHROME_COMMON_NET_CHROME_URL_FIXER_CLIENT_H_

#include "base/basictypes.h"
#include "chrome/common/net/url_fixer_client.h"

// Chrome-specific implementation of URLFixerClient.
class ChromeURLFixerClient : public URLFixerClient {
 public:
  ChromeURLFixerClient();
  virtual ~ChromeURLFixerClient();
};

#endif  // CHROME_COMMON_NET_CHROME_URL_FIXER_CLIENT_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNSTREAM_CHROMIUM_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_DOWNSTREAM_CHROMIUM_BROWSER_PROVIDER_H_

#include "ios/chrome/browser/providers/chromium_browser_provider.h"

// DownstreamChromiumBrowserProvider contains provider implementations that will
// eventually move into the upstream ChromiumBrowserProvider, but currently
// cannot move because they have internal dependencies.
class DownstreamChromiumBrowserProvider : public ChromiumBrowserProvider {
 public:
  DownstreamChromiumBrowserProvider();
  ~DownstreamChromiumBrowserProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownstreamChromiumBrowserProvider);
};

#endif  // IOS_CHROME_BROWSER_DOWNSTREAM_CHROMIUM_BROWSER_PROVIDER_H_

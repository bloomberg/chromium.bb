// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class Status;
class URLRequestContextGetter;

class ChromeAndroidImpl : public ChromeImpl {
 public:
  ChromeAndroidImpl();
  virtual ~ChromeAndroidImpl();

  virtual Status Launch(URLRequestContextGetter* context_getter,
                        int port,
                        const SyncWebSocketFactory& socket_factory,
                        const std::string& package_name);

  // Overridden from Chrome:
  virtual std::string GetOperatingSystemName() OVERRIDE;

  // Overridden from ChromeImpl:
  virtual Status Quit() OVERRIDE;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_

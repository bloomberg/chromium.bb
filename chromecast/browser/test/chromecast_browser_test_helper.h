// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_TEST_CHROMECAST_BROWSER_TEST_HELPER_H_
#define CHROMECAST_BROWSER_TEST_CHROMECAST_BROWSER_TEST_HELPER_H_

#include <memory>

class GURL;

namespace content {
class WebContents;
}

namespace chromecast {
namespace shell {

class ChromecastBrowserTestHelper {
 public:
  // Creates an implementation of ChromecastBrowserTestHelper. Allows public
  // and internal builds to instantiate different implementations.
  static std::unique_ptr<ChromecastBrowserTestHelper> Create();

  virtual ~ChromecastBrowserTestHelper() {}
  virtual content::WebContents* NavigateToURL(const GURL& url) = 0;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_TEST_CHROMECAST_BROWSER_TEST_HELPER_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_STUB_CHROME_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_STUB_CHROME_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"

class Status;
class WebView;

class StubChrome : public Chrome {
 public:
  StubChrome();
  ~StubChrome() override;

  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  const BrowserInfo* GetBrowserInfo() override;
  bool HasCrashedWebView() override;
  Status GetWebViewIds(std::list<std::string>* web_view_ids) override;
  Status GetWebViewById(const std::string& id, WebView** web_view) override;
  Status CloseWebView(const std::string& id) override;
  Status ActivateWebView(const std::string& id) override;
  std::string GetOperatingSystemName() override;
  bool IsMobileEmulationEnabled() const override;
  Status Quit() override;

 private:
  BrowserInfo browser_info_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_STUB_CHROME_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome/chrome.h"

class DevToolsHttpClient;
class JavaScriptDialogManager;
class Status;
class WebView;
class WebViewImpl;

class ChromeImpl : public Chrome {
 public:
  virtual ~ChromeImpl();

  // Overridden from Chrome:
  virtual std::string GetVersion() OVERRIDE;
  virtual int GetBuildNo() OVERRIDE;
  virtual Status GetWebViewIds(std::list<std::string>* web_view_ids) OVERRIDE;
  virtual Status GetWebViewById(const std::string& id,
                                WebView** web_view) OVERRIDE;
  virtual Status CloseWebView(const std::string& id) OVERRIDE;
  virtual Status IsJavaScriptDialogOpen(bool* is_open) OVERRIDE;
  virtual Status GetJavaScriptDialogMessage(std::string* message) OVERRIDE;
  virtual Status HandleJavaScriptDialog(
      bool accept,
      const std::string& prompt_text) OVERRIDE;

 protected:
  ChromeImpl(scoped_ptr<DevToolsHttpClient> client,
             const std::string& version,
             int build_no);

 private:
  typedef std::list<linked_ptr<WebViewImpl> > WebViewList;

  Status GetDialogManagerForOpenDialog(JavaScriptDialogManager** manager);
  Status ParseAndCheckVersion(const std::string& version);

  scoped_ptr<DevToolsHttpClient> devtools_http_client_;

  std::string version_;
  int build_no_;

  // Web views in this list are in the same order as they are opened.
  WebViewList web_views_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

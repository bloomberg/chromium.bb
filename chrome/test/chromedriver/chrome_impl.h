// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_

#include <list>
#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/web_view_delegate.h"

class JavaScriptDialogManager;
class Status;
class URLRequestContextGetter;
class WebView;
class WebViewImpl;

class ChromeImpl : public Chrome, public WebViewDelegate {
 public:
  ChromeImpl(URLRequestContextGetter* context_getter,
             int port,
             const SyncWebSocketFactory& socket_factory);
  virtual ~ChromeImpl();

  // Overridden from Chrome:
  virtual std::string GetVersion() OVERRIDE;
  virtual Status GetWebViews(std::list<WebView*>* web_views) OVERRIDE;
  virtual Status IsJavaScriptDialogOpen(bool* is_open) OVERRIDE;
  virtual Status GetJavaScriptDialogMessage(std::string* message) OVERRIDE;
  virtual Status HandleJavaScriptDialog(
      bool accept,
      const std::string& prompt_text) OVERRIDE;

  // Overridden from WebViewDelegate:
  virtual void OnWebViewClose(WebView* web_view) OVERRIDE;

 protected:
  Status Init();
  int GetPort() const;

 private:
  typedef std::map<std::string, linked_ptr<WebViewImpl> > WebViewMap;

  Status GetDialogManagerForOpenDialog(JavaScriptDialogManager** manager);
  Status ParseAndCheckVersion(const std::string& version);

  scoped_refptr<URLRequestContextGetter> context_getter_;
  int port_;
  SyncWebSocketFactory socket_factory_;
  std::string version_;
  int build_no_;
  WebViewMap web_view_map_;
};

namespace internal {

Status ParsePagesInfo(const std::string& data,
                      std::list<std::string>* page_ids);
Status ParseVersionInfo(const std::string& data,
                        std::string* version);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_

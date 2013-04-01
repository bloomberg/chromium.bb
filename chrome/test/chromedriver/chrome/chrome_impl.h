// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/web_view_delegate.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

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
  virtual int GetBuildNo() OVERRIDE;
  virtual Status GetWebViewIds(std::list<std::string>* web_view_ids) OVERRIDE;
  virtual Status GetWebViewById(const std::string& id,
                                WebView** web_view) OVERRIDE;
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
  typedef std::list<linked_ptr<WebViewImpl> > WebViewList;

  Status GetDialogManagerForOpenDialog(JavaScriptDialogManager** manager);
  Status ParseAndCheckVersion(const std::string& version);

  scoped_refptr<URLRequestContextGetter> context_getter_;
  int port_;
  SyncWebSocketFactory socket_factory_;
  std::string version_;
  int build_no_;
  // Web views in this list are in the same order as they are opened.
  WebViewList web_views_;
};

namespace internal {

struct WebViewInfo {
  enum Type {
    kPage,
    kOther
  };

  WebViewInfo(const std::string& id,
              const std::string& debugger_url,
              const std::string& url,
              Type type);

  bool IsFrontend() const;

  std::string id;
  std::string debugger_url;
  std::string url;
  Type type;
};

Status ParsePagesInfo(const std::string& data,
                      std::list<WebViewInfo>* info_list);

Status ParseVersionInfo(const std::string& data,
                        std::string* version);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

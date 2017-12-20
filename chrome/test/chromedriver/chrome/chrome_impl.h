// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "chrome/test/chromedriver/chrome/chrome.h"

struct BrowserInfo;
class DevToolsClient;
class DevToolsEventListener;
class DevToolsHttpClient;
class PortReservation;
class Status;
class WebView;
class WebViewImpl;
class WebViewsInfo;

class ChromeImpl : public Chrome {
 public:
  ~ChromeImpl() override;

  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  const BrowserInfo* GetBrowserInfo() const override;
  bool HasCrashedWebView() override;
  Status GetWebViewIdForFirstTab(std::string* web_view_id,
                                 bool w3c_complaint) override;
  Status GetWebViewIds(std::list<std::string>* web_view_ids,
                       bool w3c_compliant) override;
  Status GetWebViewById(const std::string& id, WebView** web_view) override;
  Status CloseWebView(const std::string& id) override;
  Status ActivateWebView(const std::string& id) override;
  Status SetAcceptInsecureCerts() override;
  bool IsMobileEmulationEnabled() const override;
  bool HasTouchScreen() const override;
  std::string page_load_strategy() const override;
  Status Quit() override;

 protected:
  ChromeImpl(std::unique_ptr<DevToolsHttpClient> http_client,
             std::unique_ptr<DevToolsClient> websocket_client,
             std::vector<std::unique_ptr<DevToolsEventListener>>
                 devtools_event_listeners,
             std::unique_ptr<PortReservation> port_reservation,
             std::string page_load_strategy);

  virtual Status QuitImpl() = 0;

  bool quit_;
  std::unique_ptr<DevToolsHttpClient> devtools_http_client_;
  std::unique_ptr<DevToolsClient> devtools_websocket_client_;

 private:
  typedef std::list<linked_ptr<WebViewImpl> > WebViewList;

  void UpdateWebViews(const WebViewsInfo& views_info, bool w3c_compliant);

  // Web views in this list are in the same order as they are opened.
  WebViewList web_views_;
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_event_listeners_;
  std::unique_ptr<PortReservation> port_reservation_;
  std::string page_load_strategy_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

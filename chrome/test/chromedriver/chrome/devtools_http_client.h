// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_HTTP_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_HTTP_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class DevToolsClient;
class Status;
class URLRequestContextGetter;

struct WebViewInfo {
  enum Type {
    kPage,
    kOther
  };

  WebViewInfo(const std::string& id,
              const std::string& debugger_url,
              const std::string& url,
              Type type);
  ~WebViewInfo();

  bool IsFrontend() const;

  std::string id;
  std::string debugger_url;
  std::string url;
  Type type;
};

class WebViewsInfo {
 public:
  WebViewsInfo();
  explicit WebViewsInfo(const std::vector<WebViewInfo>& info);
  ~WebViewsInfo();

  const WebViewInfo& Get(int index) const;
  size_t GetSize() const;
  const WebViewInfo* GetForId(const std::string& id) const;

 private:
  std::vector<WebViewInfo> views_info;
};

class DevToolsHttpClient {
 public:
  DevToolsHttpClient(
      int port,
      scoped_refptr<URLRequestContextGetter> context_getter,
      const SyncWebSocketFactory& socket_factory);
  ~DevToolsHttpClient();

  Status GetVersion(std::string* version);

  Status GetWebViewsInfo(WebViewsInfo* views_info);

  scoped_ptr<DevToolsClient> CreateClient(const std::string& id);

  Status CloseWebView(const std::string& id);

 private:
  Status CloseFrontends(const std::string& for_client_id);

  scoped_refptr<URLRequestContextGetter> context_getter_;
  SyncWebSocketFactory socket_factory_;
  std::string server_url_;
  std::string web_socket_url_prefix_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpClient);
};

namespace internal {
Status ParseWebViewsInfo(const std::string& data,
                         WebViewsInfo* views_info);
Status ParseVersionInfo(const std::string& data,
                        std::string* version);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_HTTP_CLIENT_H_

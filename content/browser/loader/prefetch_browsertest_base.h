// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_BROWSERTEST_BASE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/test/content_browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

namespace content {

class SignedExchangeHandlerFactory;

class PrefetchBrowserTestBase : public ContentBrowserTest {
 public:
  struct ResponseEntry {
    ResponseEntry();
    explicit ResponseEntry(
        const std::string& content,
        const std::string& content_types = "text/html",
        const std::vector<std::pair<std::string, std::string>>& headers = {});
    ~ResponseEntry();

    std::string content;
    std::string content_type;
    std::vector<std::pair<std::string, std::string>> headers;
  };

  struct ScopedSignedExchangeHandlerFactory {
    explicit ScopedSignedExchangeHandlerFactory(
        SignedExchangeHandlerFactory* factory);
    ~ScopedSignedExchangeHandlerFactory();
  };

  PrefetchBrowserTestBase();
  ~PrefetchBrowserTestBase() override;

  void SetUpOnMainThread() override;

 protected:
  void RegisterResponse(const std::string& url, const ResponseEntry& entry);

  std::unique_ptr<net::test_server::HttpResponse> ServeResponses(
      const net::test_server::HttpRequest& request);
  void WatchURLAndRunClosure(const std::string& relative_url,
                             int* visit_count,
                             base::OnceClosure closure,
                             const net::test_server::HttpRequest& request);
  void OnPrefetchURLLoaderCalled();
  void RegisterRequestMonitor(net::test_server::EmbeddedTestServer* test_server,
                              const std::string& path,
                              int* count,
                              base::RunLoop* waiter);

  void RegisterRequestHandler(
      net::test_server::EmbeddedTestServer* test_server);
  void NavigateToURLAndWaitTitle(const GURL& url, const std::string& title);
  void WaitUntilLoaded(const GURL& url);

  int prefetch_url_loader_called_ = 0;

 private:
  std::map<std::string, ResponseEntry> response_map_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchBrowserTestBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_BROWSERTEST_BASE_H_

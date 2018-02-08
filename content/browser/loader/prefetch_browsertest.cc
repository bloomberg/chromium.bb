// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"

namespace content {

class PrefetchBrowserTest : public ContentBrowserTest,
                            public testing::WithParamInterface<bool> {
 public:
  PrefetchBrowserTest() = default;
  ~PrefetchBrowserTest() = default;

  void SetUp() override {
    if (GetParam())
      feature_list_.InitWithFeatures({network::features::kNetworkService}, {});
    ContentBrowserTest::SetUp();
  }

  void RegisterResponse(const std::string& url, const std::string& content) {
    response_map_[url] = content;
  }

  std::unique_ptr<net::test_server::HttpResponse> ServeResponses(
      const net::test_server::HttpRequest& request) {
    auto found = response_map_.find(request.relative_url);
    if (found != response_map_.end()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(found->second);
      return std::move(response);
    }
    return nullptr;
  }

  void WatchURLAndRunClosure(const std::string& relative_url,
                             int* visit_count,
                             base::OnceClosure closure,
                             const net::test_server::HttpRequest& request) {
    if (request.relative_url == relative_url) {
      (*visit_count)++;
      std::move(closure).Run();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::map<std::string, std::string> response_map_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchBrowserTest);
};

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, Simple) {
  int target_fetch_count = 0;
  const std::string prefetch_url = "/prefetch.html";
  const std::string target_url = "/target.html";
  RegisterResponse(prefetch_url,
                   "<body><link rel='prefetch' href='/target.html'></body>");
  RegisterResponse(target_url, "<head><title>Prefetch Target</title></head>");

  base::RunLoop prefetch_waiter;
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      target_url, &target_fetch_count, prefetch_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PrefetchBrowserTest::ServeResponses, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, target_fetch_count);

  // Loading a page that prefetches the target URL would increment the
  // |target_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_url));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL (therefore not increment |target_fetch_count|.
  // The target content should still be read correctly.
  base::string16 title = base::ASCIIToUTF16("Prefetch Target");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), embedded_test_server()->GetURL(target_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(1, target_fetch_count);
}

INSTANTIATE_TEST_CASE_P(PrefetchBrowserTest,
                        PrefetchBrowserTest,
                        testing::Bool());

}  // namespace content

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_content_suggestions_service_proxy.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"
#include "components/ntp_snippets/remote/cached_image_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
using testing::Pointee;

namespace contextual_suggestions {

using Cluster = ntp_snippets::Cluster;
using ClustersCallback = ntp_snippets::FetchClustersCallback;

namespace {

static const std::string kTestPeekText("Test peek test");
static const std::string kValidFromUrl = "http://some.url";

class FakeContextualContentSuggestionsService
    : public ntp_snippets::ContextualContentSuggestionsService {
 public:
  FakeContextualContentSuggestionsService();
  ~FakeContextualContentSuggestionsService() override;

  void FetchContextualSuggestionClusters(const GURL& url,
                                         ClustersCallback callback) override {
    clusters_callback_ = std::move(callback);
  }

  void RunClustersCallback(std::string peek_text,
                           std::vector<Cluster> clusters) {
    std::move(clusters_callback_)
        .Run(std::move(peek_text), std::move(clusters));
  }

 private:
  ClustersCallback clusters_callback_;
};

FakeContextualContentSuggestionsService::
    FakeContextualContentSuggestionsService()
    : ContextualContentSuggestionsService(nullptr, nullptr, nullptr, nullptr) {}

FakeContextualContentSuggestionsService::
    ~FakeContextualContentSuggestionsService() {}

// GMock does not support movable-only types (Cluster).
// Instead WrappedRun is used as callback and it redirects the call to a
// method without movable-only types, which is then mocked.
class MockClustersCallback {
 public:
  void WrappedRun(std::string peek_text, std::vector<Cluster> clusters) {
    Run(peek_text, &clusters);
  }

  ClustersCallback ToOnceCallback() {
    return base::BindOnce(&MockClustersCallback::WrappedRun,
                          base::Unretained(this));
  }

  MOCK_METHOD2(Run,
               void(const std::string& peek_text,
                    std::vector<Cluster>* clusters));
};

}  // namespace

class ContextualContentSuggestionsServiceProxyTest : public testing::Test {
 public:
  void SetUp() override;

  FakeContextualContentSuggestionsService* service() { return service_.get(); }

  ContextualContentSuggestionsServiceProxy* proxy() { return proxy_.get(); }

 private:
  std::unique_ptr<FakeContextualContentSuggestionsService> service_;
  std::unique_ptr<ContextualContentSuggestionsServiceProxy> proxy_;
};

void ContextualContentSuggestionsServiceProxyTest::SetUp() {
  service_ = std::make_unique<FakeContextualContentSuggestionsService>();
  proxy_ = std::make_unique<ContextualContentSuggestionsServiceProxy>(
      service_.get());
}

TEST_F(ContextualContentSuggestionsServiceProxyTest,
       FetchSuggestionsWhenEmpty) {
  MockClustersCallback mock_cluster_callback;

  proxy()->FetchContextualSuggestions(GURL(kValidFromUrl),
                                      mock_cluster_callback.ToOnceCallback());
  EXPECT_CALL(mock_cluster_callback, Run(kTestPeekText, Pointee(IsEmpty())));
  service()->RunClustersCallback(kTestPeekText, std::vector<Cluster>());
}

// TODO(fgorski): More tests will be added, once we have the suggestions
// restructured.

}  // namespace contextual_suggestions

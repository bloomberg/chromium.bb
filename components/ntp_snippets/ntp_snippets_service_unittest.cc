// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

class SnippetObserver : public NTPSnippetsServiceObserver {
 public:
  SnippetObserver() : loaded_(false), shutdown_(false) {}
  ~SnippetObserver() override {}

  void NTPSnippetsServiceLoaded(NTPSnippetsService* service) override {
    loaded_ = true;
  }

  void NTPSnippetsServiceShutdown(NTPSnippetsService* service) override {
    shutdown_ = true;
    loaded_ = false;
  }

  bool loaded_;
  bool shutdown_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SnippetObserver);
};

class NTPSnippetsServiceTest : public testing::Test {
 public:
  NTPSnippetsServiceTest() {}
  ~NTPSnippetsServiceTest() override {}

  void SetUp() override {
    signin_client_.reset(new TestSigninClient(nullptr));
    account_tracker_.reset(new AccountTrackerService());
  }

 protected:
  scoped_ptr<NTPSnippetsService> CreateSnippetService() {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(task_runner.get());
    FakeProfileOAuth2TokenService* token_service =
        new FakeProfileOAuth2TokenService();
    FakeSigninManagerBase* signin_manager =  new FakeSigninManagerBase(
        signin_client_.get(), account_tracker_.get());

    scoped_ptr<NTPSnippetsService> service(new NTPSnippetsService(
        nullptr, task_runner.get(), std::string("fr"), nullptr,
        make_scoped_ptr(new NTPSnippetsFetcher(
            task_runner.get(), signin_manager, token_service,
            request_context_getter, base::FilePath()))));
    return service;
  }

 private:
  scoped_ptr<AccountTrackerService> account_tracker_;
  scoped_ptr<TestSigninClient> signin_client_;
  base::MessageLoop message_loop_;
  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsServiceTest);
};

TEST_F(NTPSnippetsServiceTest, Create) {
  scoped_ptr<NTPSnippetsService> service(CreateSnippetService());
  EXPECT_FALSE(service->is_loaded());
}

TEST_F(NTPSnippetsServiceTest, Loop) {
  scoped_ptr<NTPSnippetsService> service(CreateSnippetService());

  EXPECT_FALSE(service->is_loaded());

  std::string json_str(
      "{ \"recos\": [ "
      "{ \"contentInfo\": { \"url\" : \"http://localhost/foobar\" }}"
      "]}");
  EXPECT_TRUE(service->LoadFromJSONString(json_str));

  EXPECT_TRUE(service->is_loaded());

  // The same for loop without the '&' should not compile.
  for (auto& snippet : *service) {
    // Snippet here is a const.
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
  }
  // Without the const, this should not compile.
  for (const NTPSnippet& snippet : *service) {
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
  }
}

TEST_F(NTPSnippetsServiceTest, Full) {
  scoped_ptr<NTPSnippetsService> service(CreateSnippetService());

  std::string json_str(
      "{ \"recos\": [ "
      "{ \"contentInfo\": {"
      "\"url\" : \"http://localhost/foobar\","
      "\"site_title\" : \"Site Title\","
      "\"favicon_url\" : \"http://localhost/favicon\","
      "\"title\" : \"Title\","
      "\"snippet\" : \"Snippet\","
      "\"thumbnailUrl\" : \"http://localhost/salient_image\","
      "\"creationTimestampSec\" : 1448459205"
      "}}"
      "]}");
  EXPECT_TRUE(service->LoadFromJSONString(json_str));

  // The same for loop without the '&' should not compile.
  for (auto& snippet : *service) {
    // Snippet here is a const.
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.site_title(), "Site Title");
    EXPECT_EQ(snippet.favicon_url(), GURL("http://localhost/favicon"));
    EXPECT_EQ(snippet.title(), "Title");
    EXPECT_EQ(snippet.snippet(), "Snippet");
    EXPECT_EQ(snippet.salient_image_url(),
              GURL("http://localhost/salient_image"));
    base::Time then =
        base::Time::FromUTCExploded({2015, 11, 4, 25, 13, 46, 45});
    EXPECT_EQ(then, snippet.publish_date());
  }
}

TEST_F(NTPSnippetsServiceTest, ObserverNotLoaded) {
  scoped_ptr<NTPSnippetsService> service(CreateSnippetService());

  SnippetObserver observer;
  service->AddObserver(&observer);
  EXPECT_FALSE(observer.loaded_);

  std::string json_str(
      "{ \"recos\": [ "
      "{ \"contentInfo\": { \"url\" : \"http://localhost/foobar\" }}"
      "]}");
  EXPECT_TRUE(service->LoadFromJSONString(json_str));
  EXPECT_TRUE(observer.loaded_);

  service->RemoveObserver(&observer);
}

TEST_F(NTPSnippetsServiceTest, ObserverLoaded) {
  scoped_ptr<NTPSnippetsService> service(CreateSnippetService());

  std::string json_str(
      "{ \"recos\": [ "
      "{ \"contentInfo\": { \"url\" : \"http://localhost/foobar\" }}"
      "]}");
  EXPECT_TRUE(service->LoadFromJSONString(json_str));

  SnippetObserver observer;
  service->AddObserver(&observer);

  EXPECT_TRUE(observer.loaded_);

  service->RemoveObserver(&observer);
}
}  // namespace ntp_snippets

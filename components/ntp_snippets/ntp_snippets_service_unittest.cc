// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SnippetObserver : public ntp_snippets::NTPSnippetsServiceObserver {
 public:
  SnippetObserver() : loaded_(false), shutdown_(false) {}
  ~SnippetObserver() override {}

  void NTPSnippetsServiceLoaded(
      ntp_snippets::NTPSnippetsService* service) override {
    loaded_ = true;
  }

  void NTPSnippetsServiceShutdown(
      ntp_snippets::NTPSnippetsService* service) override {
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

 private:
  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsServiceTest);
};

TEST_F(NTPSnippetsServiceTest, Create) {
  std::string language_code("fr");
  scoped_ptr<ntp_snippets::NTPSnippetsService> service(
      new ntp_snippets::NTPSnippetsService(language_code));

  EXPECT_FALSE(service->is_loaded());
}

TEST_F(NTPSnippetsServiceTest, Loop) {
  std::string language_code("fr");
  scoped_ptr<ntp_snippets::NTPSnippetsService> service(
      new ntp_snippets::NTPSnippetsService(language_code));

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
  for (const ntp_snippets::NTPSnippet& snippet : *service) {
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
  }
}

TEST_F(NTPSnippetsServiceTest, Full) {
  std::string language_code("fr");
  scoped_ptr<ntp_snippets::NTPSnippetsService> service(
      new ntp_snippets::NTPSnippetsService(language_code));

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
  std::string language_code("fr");
  scoped_ptr<ntp_snippets::NTPSnippetsService> service(
      new ntp_snippets::NTPSnippetsService(language_code));

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
  std::string language_code("fr");
  scoped_ptr<ntp_snippets::NTPSnippetsService> service(
      new ntp_snippets::NTPSnippetsService(language_code));

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
}  // namespace

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_preloaded_list.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const base::FilePath kTestDataPath = base::FilePath(
    FILE_PATH_LITERAL("gen/chrome/test/data/media/engagement/preload"));

// This sample data is auto generated at build time.
const base::FilePath kSampleDataPath = kTestDataPath.AppendASCII("test.pb");

const base::FilePath kMissingFilePath = kTestDataPath.AppendASCII("missing.pb");

const base::FilePath kBadFormatFilePath =
    kTestDataPath.AppendASCII("bad_format.pb");

const base::FilePath kEmptyFilePath = kTestDataPath.AppendASCII("empty.pb");

base::FilePath GetModulePath() {
  base::FilePath module_dir;
#if defined(OS_ANDROID)
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &module_dir));
#else
  EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &module_dir));
#endif
  return module_dir;
}

}  // namespace

class MediaEngagementPreloadedListTest : public ::testing::Test {
 public:
  void SetUp() override {
    preloaded_list_ = std::make_unique<MediaEngagementPreloadedList>();
    ASSERT_FALSE(IsLoaded());
    ASSERT_TRUE(IsEmpty());
  }

  bool LoadFromFile(base::FilePath path) {
    return preloaded_list_->LoadFromFile(path);
  }

  bool CheckOriginIsPresent(GURL url) {
    return preloaded_list_->CheckOriginIsPresent(url::Origin::Create(url));
  }

  bool CheckStringIsPresent(const std::string& input) {
    return preloaded_list_->CheckStringIsPresent(input);
  }

  base::FilePath GetFilePathRelativeToModule(base::FilePath path) {
    return GetModulePath().Append(path);
  }

  bool IsLoaded() { return preloaded_list_->loaded(); }

  bool IsEmpty() { return preloaded_list_->empty(); }

 protected:
  std::unique_ptr<MediaEngagementPreloadedList> preloaded_list_;
};

TEST_F(MediaEngagementPreloadedListTest, CheckOriginIsPresent) {
  ASSERT_TRUE(LoadFromFile(GetFilePathRelativeToModule(kSampleDataPath)));
  EXPECT_TRUE(IsLoaded());
  EXPECT_FALSE(IsEmpty());

  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://example.com")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://example.org:1234")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://test--3ya.com")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("http://123.123.123.123")));

  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.org")));
  EXPECT_FALSE(CheckOriginIsPresent(GURL("http://example.com")));
  EXPECT_FALSE(CheckOriginIsPresent(GURL("http://123.123.123.124")));

  // Make sure only the full origin matches.
  EXPECT_FALSE(CheckStringIsPresent("123"));
  EXPECT_FALSE(CheckStringIsPresent("http"));
  EXPECT_FALSE(CheckStringIsPresent("example.com"));
}

TEST_F(MediaEngagementPreloadedListTest, LoadMissingFile) {
  ASSERT_FALSE(LoadFromFile(GetFilePathRelativeToModule(kMissingFilePath)));
  EXPECT_FALSE(IsLoaded());
  EXPECT_TRUE(IsEmpty());
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
}

TEST_F(MediaEngagementPreloadedListTest, LoadBadFormatFile) {
  ASSERT_FALSE(LoadFromFile(GetFilePathRelativeToModule(kBadFormatFilePath)));
  EXPECT_FALSE(IsLoaded());
  EXPECT_TRUE(IsEmpty());
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
}

TEST_F(MediaEngagementPreloadedListTest, LoadEmptyFile) {
  ASSERT_TRUE(LoadFromFile(GetFilePathRelativeToModule(kEmptyFilePath)));
  EXPECT_TRUE(IsLoaded());
  EXPECT_TRUE(IsEmpty());
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
}

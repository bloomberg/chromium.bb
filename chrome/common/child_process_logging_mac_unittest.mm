// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include <map>
#include <string>

#import <Foundation/Foundation.h>

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest ChildProcessLoggingTest;

namespace {

// Class to mock breakpad's setkeyvalue/clearkeyvalue functions needed for
// SetActiveRendererURLImpl.
// The Keys are stored in a static dictionary and methods are provided to
// verify correctness.
class MockBreakpadKeyValueStore {
 public:
  MockBreakpadKeyValueStore() {
    // Only one of these objects can be active at once.
    DCHECK(dict == NULL);
    dict = new std::map<std::string, std::string>;
  }

  ~MockBreakpadKeyValueStore() {
    // Only one of these objects can be active at once.
    DCHECK(dict != NULL);
    delete dict;
    dict = NULL;
  }

  static void SetKeyValue(const base::StringPiece& key,
                          const base::StringPiece& value) {
    DCHECK(dict != NULL);
    (*dict)[key.as_string()] = value.as_string();
  }

  static void ClearKeyValue(const base::StringPiece& key) {
    DCHECK(dict != NULL);
    dict->erase(key.as_string());
  }

  size_t CountDictionaryEntries() {
    return dict->size();
  }

  bool VerifyDictionaryContents(const std::string& url) {
    using child_process_logging::kMaxNumCrashURLChunks;
    using child_process_logging::kMaxNumURLChunkValueLength;
    using child_process_logging::kUrlChunkFormatStr;

    size_t num_url_chunks = CountDictionaryEntries();
    EXPECT_LE(num_url_chunks, kMaxNumCrashURLChunks);

    std::string accumulated_url;
    for (size_t i = 0; i < num_url_chunks; ++i) {
      // URL chunk names are 1-based.
      std::string key = base::StringPrintf(kUrlChunkFormatStr, i + 1);
      std::string value = (*dict)[key];
      EXPECT_GT(value.length(), 0u);
      EXPECT_LE(value.length(),
                static_cast<size_t>(kMaxNumURLChunkValueLength));
      accumulated_url += value;
    }

    return url == accumulated_url;
  }

 private:
  static std::map<std::string, std::string>* dict;
  DISALLOW_COPY_AND_ASSIGN(MockBreakpadKeyValueStore);
};

// static
std::map<std::string, std::string>* MockBreakpadKeyValueStore::dict;

}  // namespace

// Call through to SetActiveURLImpl using the functions from
// MockBreakpadKeyValueStore.
void SetActiveURLWithMock(const GURL& url) {
  using child_process_logging::SetActiveURLImpl;

  base::debug::SetCrashKeyValueFuncT setFunc =
      MockBreakpadKeyValueStore::SetKeyValue;
  base::debug::ClearCrashKeyValueFuncT clearFunc =
      MockBreakpadKeyValueStore::ClearKeyValue;

  SetActiveURLImpl(url, setFunc, clearFunc);
}

TEST_F(ChildProcessLoggingTest, TestUrlSplitting) {
  using child_process_logging::kMaxNumCrashURLChunks;
  using child_process_logging::kMaxNumURLChunkValueLength;

  const std::string short_url("http://abc/");
  std::string long_url("http://");
  std::string overflow_url("http://");

  long_url += std::string(kMaxNumURLChunkValueLength * 2, 'a');
  long_url += "/";

  int max_num_chars_stored_in_dump = kMaxNumURLChunkValueLength *
      kMaxNumCrashURLChunks;
  overflow_url += std::string(max_num_chars_stored_in_dump + 1, 'a');
  overflow_url += "/";

  // Check that Clearing NULL URL works.
  MockBreakpadKeyValueStore mock;
  SetActiveURLWithMock(GURL());
  EXPECT_EQ(0u, mock.CountDictionaryEntries());

  // Check that we can set a URL.
  SetActiveURLWithMock(GURL(short_url.c_str()));
  EXPECT_TRUE(mock.VerifyDictionaryContents(short_url));
  EXPECT_EQ(1u, mock.CountDictionaryEntries());
  SetActiveURLWithMock(GURL());
  EXPECT_EQ(0u, mock.CountDictionaryEntries());

  // Check that we can replace a long url with a short url.
  SetActiveURLWithMock(GURL(long_url.c_str()));
  EXPECT_TRUE(mock.VerifyDictionaryContents(long_url));
  SetActiveURLWithMock(GURL(short_url.c_str()));
  EXPECT_TRUE(mock.VerifyDictionaryContents(short_url));
  SetActiveURLWithMock(GURL());
  EXPECT_EQ(0u, mock.CountDictionaryEntries());


  // Check that overflow works correctly.
  SetActiveURLWithMock(GURL(overflow_url.c_str()));
  EXPECT_TRUE(mock.VerifyDictionaryContents(
      overflow_url.substr(0, max_num_chars_stored_in_dump)));
  SetActiveURLWithMock(GURL());
  EXPECT_EQ(0u, mock.CountDictionaryEntries());
}

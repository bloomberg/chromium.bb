// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/download/download_query.h"
#include "content/public/test/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using base::Time;
using base::Value;
using content::DownloadItem;
typedef DownloadQuery::DownloadVector DownloadVector;

namespace {

static const int kSomeKnownTime = 1355864160;
static const char kSomeKnownTime8601[] = "2012-12-18T20:56:0";
static const char k8601Suffix[] = ".000Z";

bool IdNotEqual(int not_id, const DownloadItem& item) {
  return item.GetId() != not_id;
}

bool AlwaysReturn(bool result, const DownloadItem& item) {
  return result;
}

}  // anonymous namespace

class DownloadQueryTest : public testing::Test {
 public:
  DownloadQueryTest() {}

  virtual ~DownloadQueryTest() {}

  virtual void TearDown() {
    STLDeleteElements(&mocks_);
  }

  void CreateMocks(int count) {
    for (int i = 0; i < count; ++i) {
      mocks_.push_back(new content::MockDownloadItem());
      EXPECT_CALL(mock(mocks_.size() - 1), GetId()).WillRepeatedly(Return(
          mocks_.size() - 1));
    }
  }

  content::MockDownloadItem& mock(int index) { return *mocks_[index]; }

  DownloadQuery* query() { return &query_; }

  template<typename ValueType> void AddFilter(
      DownloadQuery::FilterType name, ValueType value);

  void Search() {
    query_.Search(mocks_.begin(), mocks_.end(), &results_);
  }

  DownloadVector* results() { return &results_; }

  // Filter tests generally contain 2 items. mock(0) matches the filter, mock(1)
  // does not.
  void ExpectStandardFilterResults() {
    Search();
    ASSERT_EQ(1U, results()->size());
    ASSERT_EQ(0, results()->at(0)->GetId());
  }

  // If no sorters distinguish between two items, then DownloadQuery sorts by ID
  // ascending. In order to test that a sorter distinguishes between two items,
  // the sorter must sort them by ID descending.
  void ExpectSortInverted() {
    Search();
    ASSERT_EQ(2U, results()->size());
    ASSERT_EQ(1, results()->at(0)->GetId());
    ASSERT_EQ(0, results()->at(1)->GetId());
  }

 private:
  std::vector<content::MockDownloadItem*> mocks_;
  DownloadQuery query_;
  DownloadVector results_;

  DISALLOW_COPY_AND_ASSIGN(DownloadQueryTest);
};

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, bool cpp_value) {
  scoped_ptr<base::Value> value(Value::CreateBooleanValue(cpp_value));
  CHECK(query_.AddFilter(name, *value.get()));
}

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, int cpp_value) {
  scoped_ptr<base::Value> value(Value::CreateIntegerValue(cpp_value));
  CHECK(query_.AddFilter(name, *value.get()));
}

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, const char* cpp_value) {
  scoped_ptr<base::Value> value(Value::CreateStringValue(cpp_value));
  CHECK(query_.AddFilter(name, *value.get()));
}

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, std::string cpp_value) {
  scoped_ptr<base::Value> value(Value::CreateStringValue(cpp_value));
  CHECK(query_.AddFilter(name, *value.get()));
}

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, const char16* cpp_value) {
  scoped_ptr<base::Value> value(Value::CreateStringValue(string16(cpp_value)));
  CHECK(query_.AddFilter(name, *value.get()));
}

#if defined(OS_WIN)
template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, std::wstring cpp_value) {
  scoped_ptr<base::Value> value(Value::CreateStringValue(cpp_value));
  CHECK(query_.AddFilter(name, *value.get()));
}
#endif

TEST_F(DownloadQueryTest, DownloadQueryTest_ZeroItems) {
  Search();
  EXPECT_EQ(0U, results()->size());
}

TEST_F(DownloadQueryTest, DownloadQueryTest_InvalidFilter) {
  scoped_ptr<base::Value> value(Value::CreateIntegerValue(0));
  EXPECT_FALSE(query()->AddFilter(
      static_cast<DownloadQuery::FilterType>(kint32max),
      *value.get()));
}

TEST_F(DownloadQueryTest, DownloadQueryTest_EmptyQuery) {
  CreateMocks(2);
  Search();
  ASSERT_EQ(2U, results()->size());
  ASSERT_EQ(0, results()->at(0)->GetId());
  ASSERT_EQ(1, results()->at(1)->GetId());
}

TEST_F(DownloadQueryTest, DownloadQueryTest_Limit) {
  CreateMocks(2);
  query()->Limit(1);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterGenericQueryFilename) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetBrowserContext()).WillRepeatedly(Return(
      static_cast<content::BrowserContext*>(NULL)));
  EXPECT_CALL(mock(1), GetBrowserContext()).WillRepeatedly(Return(
      static_cast<content::BrowserContext*>(NULL)));
  FilePath match_filename(FILE_PATH_LITERAL("query"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      match_filename));
  FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_QUERY, "query");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterGenericQueryUrl) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetBrowserContext()).WillRepeatedly(Return(
      static_cast<content::BrowserContext*>(NULL)));
  EXPECT_CALL(mock(1), GetBrowserContext()).WillRepeatedly(Return(
      static_cast<content::BrowserContext*>(NULL)));
  FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL match_url("http://query.com/query");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(match_url));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_QUERY, "query");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterGenericQueryFilenameI18N) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetBrowserContext()).WillRepeatedly(Return(
      static_cast<content::BrowserContext*>(NULL)));
  EXPECT_CALL(mock(1), GetBrowserContext()).WillRepeatedly(Return(
      static_cast<content::BrowserContext*>(NULL)));
  const FilePath::StringType kTestString(
#if defined(OS_POSIX)
      "/\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd"
#elif defined(OS_WIN)
      L"/\x4f60\x597d\x4f60\x597d"
#endif
      );
  FilePath match_filename(kTestString);
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      match_filename));
  FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_QUERY, kTestString);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterFilenameRegex) {
  CreateMocks(2);
  FilePath match_filename(FILE_PATH_LITERAL("query"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      match_filename));
  FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  AddFilter(DownloadQuery::FILTER_FILENAME_REGEX, "y");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortFilename) {
  CreateMocks(2);
  FilePath b_filename(FILE_PATH_LITERAL("b"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      b_filename));
  FilePath a_filename(FILE_PATH_LITERAL("a"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      a_filename));
  query()->AddSorter(DownloadQuery::SORT_FILENAME, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterFilename) {
  CreateMocks(2);
  FilePath match_filename(FILE_PATH_LITERAL("query"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      match_filename));
  FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  AddFilter(DownloadQuery::FILTER_FILENAME, match_filename.value().c_str());
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterUrlRegex) {
  CreateMocks(2);
  GURL match_url("http://query.com/query");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(match_url));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_URL_REGEX, "query");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortUrl) {
  CreateMocks(2);
  GURL b_url("http://example.com/b");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(b_url));
  GURL a_url("http://example.com/a");
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(a_url));
  query()->AddSorter(DownloadQuery::SORT_URL, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterUrl) {
  CreateMocks(2);
  GURL match_url("http://query.com/query");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(match_url));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_URL, match_url.spec().c_str());
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterCallback) {
  CreateMocks(2);
  CHECK(query()->AddFilter(base::Bind(&IdNotEqual, 1)));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterBytesReceived) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(1));
  AddFilter(DownloadQuery::FILTER_BYTES_RECEIVED, 0);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortBytesReceived) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(1));
  query()->AddSorter(DownloadQuery::SORT_BYTES_RECEIVED,
                     DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterDangerAccepted) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType()).WillRepeatedly(Return(
      content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  EXPECT_CALL(mock(1), GetDangerType()).WillRepeatedly(Return(
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  AddFilter(DownloadQuery::FILTER_DANGER_ACCEPTED, true);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortDangerAccepted) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType()).WillRepeatedly(Return(
      content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  EXPECT_CALL(mock(1), GetDangerType()).WillRepeatedly(Return(
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  query()->AddSorter(DownloadQuery::SORT_DANGER_ACCEPTED,
                     DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterExists) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetFileExternallyRemoved()).WillRepeatedly(Return(
      false));
  EXPECT_CALL(mock(1), GetFileExternallyRemoved()).WillRepeatedly(Return(
      true));
  AddFilter(DownloadQuery::FILTER_EXISTS, true);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortExists) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetFileExternallyRemoved()).WillRepeatedly(Return(
      false));
  EXPECT_CALL(mock(1), GetFileExternallyRemoved()).WillRepeatedly(Return(
      true));
  query()->AddSorter(DownloadQuery::SORT_EXISTS,
                     DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterMime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetMimeType()).WillRepeatedly(Return("text"));
  EXPECT_CALL(mock(1), GetMimeType()).WillRepeatedly(Return("image"));
  AddFilter(DownloadQuery::FILTER_MIME, "text");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortMime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetMimeType()).WillRepeatedly(Return("b"));
  EXPECT_CALL(mock(1), GetMimeType()).WillRepeatedly(Return("a"));
  query()->AddSorter(DownloadQuery::SORT_MIME, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterPaused) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), IsPaused()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock(1), IsPaused()).WillRepeatedly(Return(false));
  AddFilter(DownloadQuery::FILTER_PAUSED, true);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortPaused) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), IsPaused()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock(1), IsPaused()).WillRepeatedly(Return(false));
  query()->AddSorter(DownloadQuery::SORT_PAUSED, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterStartedAfter) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 1)));
  AddFilter(DownloadQuery::FILTER_STARTED_AFTER,
            std::string(kSomeKnownTime8601) + "1" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterStartedBefore) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  AddFilter(DownloadQuery::FILTER_STARTED_BEFORE,
            std::string(kSomeKnownTime8601) + "4" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterStartTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  AddFilter(DownloadQuery::FILTER_START_TIME,
            std::string(kSomeKnownTime8601) + "2" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortStartTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  query()->AddSorter(DownloadQuery::SORT_START_TIME, DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterEndedAfter) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 1)));
  AddFilter(DownloadQuery::FILTER_ENDED_AFTER,
            std::string(kSomeKnownTime8601) + "1" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterEndedBefore) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  AddFilter(DownloadQuery::FILTER_ENDED_BEFORE,
            std::string(kSomeKnownTime8601) + "4" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterEndTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  AddFilter(DownloadQuery::FILTER_END_TIME,
            std::string(kSomeKnownTime8601) + "2" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortEndTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  query()->AddSorter(DownloadQuery::SORT_END_TIME, DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesGreater) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(1));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_GREATER, 1);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesLess) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(4));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_LESS, 4);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytes) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(4));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES, 2);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortTotalBytes) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(4));
  query()->AddSorter(DownloadQuery::SORT_TOTAL_BYTES,
                     DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterState) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetState()).WillRepeatedly(Return(
      DownloadItem::IN_PROGRESS));
  EXPECT_CALL(mock(1), GetState()).WillRepeatedly(Return(
      DownloadItem::CANCELLED));
  query()->AddFilter(DownloadItem::IN_PROGRESS);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortState) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetState()).WillRepeatedly(Return(
      DownloadItem::IN_PROGRESS));
  EXPECT_CALL(mock(1), GetState()).WillRepeatedly(Return(
      DownloadItem::CANCELLED));
  query()->AddSorter(DownloadQuery::SORT_STATE, DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterDanger) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType()).WillRepeatedly(Return(
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(mock(1), GetDangerType()).WillRepeatedly(Return(
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  query()->AddFilter(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortDanger) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType()).WillRepeatedly(Return(
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(mock(1), GetDangerType()).WillRepeatedly(Return(
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  query()->AddSorter(DownloadQuery::SORT_DANGER, DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_DefaultSortById1) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(0));
  query()->AddSorter(DownloadQuery::SORT_BYTES_RECEIVED,
                     DownloadQuery::ASCENDING);
  Search();
  ASSERT_EQ(2U, results()->size());
  EXPECT_EQ(0, results()->at(0)->GetId());
  EXPECT_EQ(1, results()->at(1)->GetId());
}

TEST_F(DownloadQueryTest, DownloadQueryTest_DefaultSortById2) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(0));
  query()->AddSorter(DownloadQuery::SORT_BYTES_RECEIVED,
                     DownloadQuery::DESCENDING);
  Search();
  ASSERT_EQ(2U, results()->size());
  EXPECT_EQ(0, results()->at(0)->GetId());
  EXPECT_EQ(1, results()->at(1)->GetId());
}

TEST_F(DownloadQueryTest, DownloadQueryFilterPerformance) {
  static const int kNumItems = 100;
  static const int kNumFilters = 100;
  CreateMocks(kNumItems);
  for (size_t i = 0; i < (kNumFilters - 1); ++i) {
    query()->AddFilter(base::Bind(&AlwaysReturn, true));
  }
  query()->AddFilter(base::Bind(&AlwaysReturn, false));
  base::Time start = base::Time::Now();
  Search();
  base::Time end = base::Time::Now();
  double nanos = (end - start).InMillisecondsF() * 1000.0 * 1000.0;
  double nanos_per_item = nanos / static_cast<double>(kNumItems);
  double nanos_per_item_per_filter = nanos_per_item
    / static_cast<double>(kNumFilters);
  std::cout << "Search took " << nanos_per_item_per_filter
            << " nanoseconds per item per filter.\n";
}

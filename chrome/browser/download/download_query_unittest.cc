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
    DownloadQuery::FilterType name, const char16* cpp_value) {
  scoped_ptr<base::Value> value(Value::CreateStringValue(string16(cpp_value)));
  CHECK(query_.AddFilter(name, *value.get()));
}

TEST_F(DownloadQueryTest, DownloadQueryEmptyNoItems) {
  Search();
  EXPECT_EQ(0U, results()->size());
}

TEST_F(DownloadQueryTest, DownloadQueryEmptySomeItems) {
  CreateMocks(3);
  Search();
  EXPECT_EQ(3U, results()->size());
}

TEST_F(DownloadQueryTest, DownloadQueryInvalidFilters) {
  scoped_ptr<base::Value> value(Value::CreateIntegerValue(0));
  EXPECT_FALSE(query()->AddFilter(
      static_cast<DownloadQuery::FilterType>(kint32max),
      *value.get()));
}

TEST_F(DownloadQueryTest, DownloadQueryLimit) {
  CreateMocks(2);
  query()->Limit(1);
  Search();
  EXPECT_EQ(1U, results()->size());
}

// Syntactic sugar for an expression version of the switch-case statement.
// Cf. Lisp's |case| form.
#define SWITCH2(_index, _col1, _ret1, _default) \
  ((_index == (_col1)) ? _ret1 : _default)
#define SWITCH3(_index, _col1, _ret1, _col2, _ret2, _default) \
  SWITCH2(_index, _col1, _ret1, SWITCH2(_index, _col2, _ret2, _default))
#define SWITCH4(_index, _col1, _ret1, _col2, _ret2, _col3, _ret3, _default) \
  SWITCH3(_index, _col1, _ret1, _col2, _ret2, \
      SWITCH2(_index, _col3, _ret3, _default))

TEST_F(DownloadQueryTest, DownloadQueryAllFilters) {
  // Set up mocks such that only mock(0) matches all filters, and every other
  // mock fails a different filter (or two for GREATER/LESS filters).
  static const size_t kNumItems = 19;
  CreateMocks(kNumItems);
  FilePath refail_filename(FILE_PATH_LITERAL("z"));
  FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  FilePath match_filename(FILE_PATH_LITERAL("match"));
  GURL refail_url("http://z.com/");
  GURL fail_url("http://example.com/fail");
  GURL match_url("http://example.com/match");
  // Picture a 2D matrix. The rows are MockDownloadItems and the columns are
  // filter types. Every cell contains a value that matches all filters, except
  // for the diagonal. Every item matches all the filters except one filter,
  // which it fails, except one item, which matches all the filters without
  // exception. Each mocked method is used to test (corresponds to) one or more
  // filter types (columns). For example, GetTotalBytes() is used to test
  // FILTER_TOTAL_BYTES_GREATER, FILTER_TOTAL_BYTES_LESS, and
  // FILTER_TOTAL_BYTES, so it uses 3 columns: it returns 1 for row (item) 11,
  // it returns 4 for row 12, 3 for 13, and it returns 2 for all other rows
  // (items).
  for (size_t i = 0; i < kNumItems; ++i) {
    EXPECT_CALL(mock(i), GetId()).WillRepeatedly(Return(i));
    EXPECT_CALL(mock(i), GetReceivedBytes()).WillRepeatedly(Return(SWITCH2(i,
        1, 2,
           1)));
    EXPECT_CALL(mock(i), GetSafetyState()).WillRepeatedly(Return(SWITCH2(i,
        2, DownloadItem::DANGEROUS,
           DownloadItem::DANGEROUS_BUT_VALIDATED)));
    EXPECT_CALL(mock(i), GetFullPath()).WillRepeatedly(ReturnRef(SWITCH3(i,
        3, refail_filename,
        4, fail_filename,
           match_filename)));
    EXPECT_CALL(mock(i), GetMimeType()).WillRepeatedly(Return(SWITCH2(i,
        5, "image",
           "text")));
    EXPECT_CALL(mock(i), IsPaused()).WillRepeatedly(Return(SWITCH2(i,
        6, false,
           true)));
    EXPECT_CALL(mock(i), MatchesQuery(_)).WillRepeatedly(Return(SWITCH2(i,
        7, false,
           true)));
    EXPECT_CALL(mock(i), GetStartTime()).WillRepeatedly(Return(SWITCH4(i,
        8,  base::Time::FromTimeT(1),
        9,  base::Time::FromTimeT(4),
        10, base::Time::FromTimeT(3),
            base::Time::FromTimeT(2))));
    EXPECT_CALL(mock(i), GetTotalBytes()).WillRepeatedly(Return(SWITCH4(i,
        11, 1,
        12, 4,
        13, 3,
            2)));
    EXPECT_CALL(mock(i), GetOriginalUrl()).WillRepeatedly(ReturnRef(SWITCH3(i,
        14, refail_url,
        15, fail_url,
            match_url)));
    EXPECT_CALL(mock(i), GetState()).WillRepeatedly(Return(SWITCH2(i,
        17, DownloadItem::CANCELLED,
            DownloadItem::IN_PROGRESS)));
    EXPECT_CALL(mock(i), GetDangerType()).WillRepeatedly(Return(SWITCH2(i,
        18, content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)));
  }
  for (size_t i = 0; i < kNumItems; ++i) {
    switch (i) {
      case 0: break;
      case 1: AddFilter(DownloadQuery::FILTER_BYTES_RECEIVED, 1); break;
      case 2: AddFilter(DownloadQuery::FILTER_DANGER_ACCEPTED, true);
              break;
      case 3: AddFilter(DownloadQuery::FILTER_FILENAME_REGEX, "a"); break;
      case 4: AddFilter(DownloadQuery::FILTER_FILENAME,
                        match_filename.value().c_str()); break;
      case 5: AddFilter(DownloadQuery::FILTER_MIME, "text"); break;
      case 6: AddFilter(DownloadQuery::FILTER_PAUSED, true); break;
      case 7: AddFilter(DownloadQuery::FILTER_QUERY, ""); break;
      case 8: AddFilter(DownloadQuery::FILTER_STARTED_AFTER, 1000); break;
      case 9: AddFilter(DownloadQuery::FILTER_STARTED_BEFORE, 4000);
              break;
      case 10: AddFilter(DownloadQuery::FILTER_START_TIME, 2000); break;
      case 11: AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_GREATER, 1);
               break;
      case 12: AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_LESS, 4);
               break;
      case 13: AddFilter(DownloadQuery::FILTER_TOTAL_BYTES, 2); break;
      case 14: AddFilter(DownloadQuery::FILTER_URL_REGEX, "example");
               break;
      case 15: AddFilter(DownloadQuery::FILTER_URL,
                         match_url.spec().c_str()); break;
      case 16: CHECK(query()->AddFilter(base::Bind(&IdNotEqual, 16))); break;
      case 17: query()->AddFilter(DownloadItem::IN_PROGRESS); break;
      case 18: query()->AddFilter(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
        break;
      default: NOTREACHED(); break;
    }
    Search();
    ASSERT_EQ(kNumItems - i, results()->size())
      << "Failing filter: " << i;
    if (i > 0) {
      ASSERT_EQ(0, results()->at(0)->GetId())
        << "Failing filter: " << i;
      for (size_t j = 1; j < kNumItems - i; ++j) {
        ASSERT_EQ(static_cast<int32>(j + i), results()->at(j)->GetId())
          << "Failing filter: " << i;
      }
    }
  }
}

TEST_F(DownloadQueryTest, DownloadQuerySortBytesReceived) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(1));
  query()->AddSorter(
      DownloadQuery::SORT_BYTES_RECEIVED, DownloadQuery::DESCENDING);
  Search();
  EXPECT_EQ(1, results()->at(0)->GetReceivedBytes());
  EXPECT_EQ(0, results()->at(1)->GetReceivedBytes());
}

TEST_F(DownloadQueryTest, DownloadQuerySortDanger) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType()).WillRepeatedly(Return(
        content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  EXPECT_CALL(mock(1), GetDangerType()).WillRepeatedly(Return(
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  query()->AddSorter(
      DownloadQuery::SORT_DANGER, DownloadQuery::ASCENDING);
  Search();
  EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            results()->at(0)->GetDangerType());
  EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            results()->at(1)->GetDangerType());
}

TEST_F(DownloadQueryTest, DownloadQuerySortDangerAccepted) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetSafetyState()).WillRepeatedly(Return(
        DownloadItem::DANGEROUS));
  EXPECT_CALL(mock(1), GetSafetyState()).WillRepeatedly(Return(
        DownloadItem::DANGEROUS_BUT_VALIDATED));
  query()->AddSorter(
      DownloadQuery::SORT_DANGER_ACCEPTED, DownloadQuery::DESCENDING);
  Search();
  EXPECT_EQ(DownloadItem::DANGEROUS_BUT_VALIDATED,
            results()->at(0)->GetSafetyState());
  EXPECT_EQ(DownloadItem::DANGEROUS, results()->at(1)->GetSafetyState());
}

TEST_F(DownloadQueryTest, DownloadQuerySortFilename) {
  CreateMocks(2);
  FilePath a_filename(FILE_PATH_LITERAL("a"));
  FilePath b_filename(FILE_PATH_LITERAL("b"));
  EXPECT_CALL(mock(0), GetFullPath()).WillRepeatedly(ReturnRef(b_filename));
  EXPECT_CALL(mock(1), GetFullPath()).WillRepeatedly(ReturnRef(a_filename));
  query()->AddSorter(
      DownloadQuery::SORT_FILENAME, DownloadQuery::ASCENDING);
  Search();
  EXPECT_EQ(a_filename, results()->at(0)->GetFullPath());
  EXPECT_EQ(b_filename, results()->at(1)->GetFullPath());
}

TEST_F(DownloadQueryTest, DownloadQuerySortMime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetMimeType()).WillRepeatedly(Return("a"));
  EXPECT_CALL(mock(1), GetMimeType()).WillRepeatedly(Return("b"));
  query()->AddSorter(
      DownloadQuery::SORT_MIME, DownloadQuery::DESCENDING);
  Search();
  EXPECT_EQ("b", results()->at(0)->GetMimeType());
  EXPECT_EQ("a", results()->at(1)->GetMimeType());
}

TEST_F(DownloadQueryTest, DownloadQuerySortPaused) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), IsPaused()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock(1), IsPaused()).WillRepeatedly(Return(false));
  query()->AddSorter(
      DownloadQuery::SORT_PAUSED, DownloadQuery::ASCENDING);
  Search();
  EXPECT_FALSE(results()->at(0)->IsPaused());
  EXPECT_TRUE(results()->at(1)->IsPaused());
}

TEST_F(DownloadQueryTest, DownloadQuerySortStartTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
        base::Time::FromTimeT(0)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
        base::Time::FromTimeT(1)));
  query()->AddSorter(
      DownloadQuery::SORT_START_TIME, DownloadQuery::DESCENDING);
  Search();
  EXPECT_EQ(base::Time::FromTimeT(1), results()->at(0)->GetStartTime());
  EXPECT_EQ(base::Time::FromTimeT(0), results()->at(1)->GetStartTime());
}

TEST_F(DownloadQueryTest, DownloadQuerySortState) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetState()).WillRepeatedly(Return(
        DownloadItem::IN_PROGRESS));
  EXPECT_CALL(mock(1), GetState()).WillRepeatedly(Return(
        DownloadItem::COMPLETE));
  query()->AddSorter(
      DownloadQuery::SORT_STATE, DownloadQuery::ASCENDING);
  Search();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, results()->at(0)->GetState());
  EXPECT_EQ(DownloadItem::COMPLETE, results()->at(1)->GetState());
}

TEST_F(DownloadQueryTest, DownloadQuerySortTotalBytes) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(1));
  query()->AddSorter(
      DownloadQuery::SORT_TOTAL_BYTES, DownloadQuery::DESCENDING);
  Search();
  EXPECT_EQ(1, results()->at(0)->GetTotalBytes());
  EXPECT_EQ(0, results()->at(1)->GetTotalBytes());
}

TEST_F(DownloadQueryTest, DownloadQuerySortUrl) {
  CreateMocks(2);
  GURL a_url("http://example.com/a");
  GURL b_url("http://example.com/b");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(b_url));
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(a_url));
  query()->AddSorter(
      DownloadQuery::SORT_URL, DownloadQuery::ASCENDING);
  Search();
  EXPECT_EQ(a_url, results()->at(0)->GetOriginalUrl());
  EXPECT_EQ(b_url, results()->at(1)->GetOriginalUrl());
}

TEST_F(DownloadQueryTest, DownloadQuerySortId) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(0), GetId()).WillRepeatedly(Return(1));
  EXPECT_CALL(mock(1), GetId()).WillRepeatedly(Return(0));
  query()->AddSorter(
      DownloadQuery::SORT_BYTES_RECEIVED, DownloadQuery::DESCENDING);
  Search();
  EXPECT_EQ(0, results()->at(0)->GetId());
  EXPECT_EQ(1, results()->at(1)->GetId());
}

TEST_F(DownloadQueryTest, DownloadQueryFilterPerformance) {
  static const int kNumItems = 10000;
  static const int kNumFilters = 1000;
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

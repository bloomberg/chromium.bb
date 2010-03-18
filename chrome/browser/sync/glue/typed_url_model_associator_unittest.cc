// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::TypedUrlModelAssociator;

class TypedUrlModelAssociatorTest : public testing::Test {
 public:
  static history::URLRow MakeTypedUrlRow(const char* url,
                                         const char* title,
                                         int visit_count,
                                         int typed_count,
                                         int64 last_visit,
                                         bool hidden) {
    GURL gurl(url);
    history::URLRow history_url(gurl);
    history_url.set_title(UTF8ToWide(title));
    history_url.set_visit_count(visit_count);
    history_url.set_typed_count(typed_count);
    history_url.set_last_visit(
        base::Time::FromInternalValue(last_visit));
    history_url.set_hidden(hidden);
    return history_url;
  }

  static sync_pb::TypedUrlSpecifics MakeTypedUrlSpecifics(const char* url,
                                                          const char* title,
                                                          int visit_count,
                                                          int typed_count,
                                                          int64 last_visit,
                                                          bool hidden) {
    sync_pb::TypedUrlSpecifics typed_url;
    typed_url.set_url(url);
    typed_url.set_title(title);
    typed_url.set_visit_count(visit_count);
    typed_url.set_typed_count(typed_count);
    typed_url.set_last_visit(last_visit);
    typed_url.set_hidden(hidden);
    return typed_url;
  }

  static bool URLsEqual(history::URLRow& lhs, history::URLRow& rhs) {
    return (lhs.url().spec().compare(rhs.url().spec()) == 0) &&
           (lhs.title().compare(rhs.title()) == 0) &&
           (lhs.visit_count() == rhs.visit_count()) &&
           (lhs.typed_count() == rhs.typed_count()) &&
           (lhs.last_visit() == rhs.last_visit()) &&
           (lhs.hidden() == rhs.hidden());
  }
};

TEST_F(TypedUrlModelAssociatorTest, MergeUrls) {
  history::URLRow row1(MakeTypedUrlRow("http://pie.com/", "pie",
                                       1, 2, 3, false));
  sync_pb::TypedUrlSpecifics specs1(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          1, 2, 3, false));
  history::URLRow new_row1(GURL("http://pie.com/"));
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_NONE,
            TypedUrlModelAssociator::MergeUrls(specs1, row1, &new_row1));

  history::URLRow row2(MakeTypedUrlRow("http://pie.com/", "pie",
                                       1, 2, 3, false));
  sync_pb::TypedUrlSpecifics specs2(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          1, 2, 4, true));
  history::URLRow expected2(MakeTypedUrlRow("http://pie.com/", "pie",
                                            1, 2, 4, true));
  history::URLRow new_row2(GURL("http://pie.com/"));
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_ROW_CHANGED,
            TypedUrlModelAssociator::MergeUrls(specs2, row2, &new_row2));
  EXPECT_TRUE(URLsEqual(new_row2, expected2));

  history::URLRow row3(MakeTypedUrlRow("http://pie.com/", "pie",
                                       1, 2, 3, false));
  sync_pb::TypedUrlSpecifics specs3(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie2",
                                                          1, 2, 4, true));
  history::URLRow expected3(MakeTypedUrlRow("http://pie.com/", "pie2",
                                            1, 2, 4, true));
  history::URLRow new_row3(GURL("http://pie.com/"));
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_ROW_CHANGED |
            TypedUrlModelAssociator::DIFF_TITLE_CHANGED,
            TypedUrlModelAssociator::MergeUrls(specs3, row3, &new_row3));
  EXPECT_TRUE(URLsEqual(new_row3, expected3));

  history::URLRow row4(MakeTypedUrlRow("http://pie.com/", "pie",
                                       1, 2, 4, false));
  sync_pb::TypedUrlSpecifics specs4(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie2",
                                                          1, 2, 3, true));
  history::URLRow expected4(MakeTypedUrlRow("http://pie.com/", "pie",
                                            1, 2, 4, false));
  history::URLRow new_row4(GURL("http://pie.com/"));
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_NODE_CHANGED,
            TypedUrlModelAssociator::MergeUrls(specs4, row4, &new_row4));
  EXPECT_TRUE(URLsEqual(new_row4, expected4));

  history::URLRow row5(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 1, 3, false));
  sync_pb::TypedUrlSpecifics specs5(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          1, 2, 3, false));
  history::URLRow expected5(MakeTypedUrlRow("http://pie.com/", "pie",
                                            2, 2, 3, false));
  history::URLRow new_row5(GURL("http://pie.com/"));
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_ROW_CHANGED |
            TypedUrlModelAssociator::DIFF_NODE_CHANGED,
            TypedUrlModelAssociator::MergeUrls(specs5, row5, &new_row5));
  EXPECT_TRUE(URLsEqual(new_row5, expected5));

}

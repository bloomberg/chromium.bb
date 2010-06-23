// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
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
                                         int typed_count,
                                         int64 last_visit,
                                         bool hidden,
                                         history::VisitVector* visits) {
    GURL gurl(url);
    history::URLRow history_url(gurl);
    history_url.set_title(UTF8ToUTF16(title));
    history_url.set_typed_count(typed_count);
    history_url.set_last_visit(
        base::Time::FromInternalValue(last_visit));
    history_url.set_hidden(hidden);
    visits->push_back(history::VisitRow(
        history_url.id(), history_url.last_visit(), 0, 0, 0));
    history_url.set_visit_count(visits->size());
    return history_url;
  }

  static sync_pb::TypedUrlSpecifics MakeTypedUrlSpecifics(const char* url,
                                                          const char* title,
                                                          int typed_count,
                                                          int64 last_visit,
                                                          bool hidden) {
    sync_pb::TypedUrlSpecifics typed_url;
    typed_url.set_url(url);
    typed_url.set_title(title);
    typed_url.set_typed_count(typed_count);
    typed_url.set_hidden(hidden);
    typed_url.add_visit(last_visit);
    return typed_url;
  }

  static bool URLsEqual(history::URLRow& lhs, history::URLRow& rhs) {
    return (lhs.url().spec().compare(rhs.url().spec()) == 0) &&
           (lhs.title().compare(rhs.title()) == 0) &&
           (lhs.visit_count() == rhs.visit_count()) &&
           (lhs.typed_count() == rhs.typed_count()) &&
           (lhs.hidden() == rhs.hidden());
  }
};

TEST_F(TypedUrlModelAssociatorTest, MergeUrls) {
  history::VisitVector visits1;
  history::URLRow row1(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 3, false, &visits1));
  sync_pb::TypedUrlSpecifics specs1(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          2, 3, false));
  history::URLRow new_row1(GURL("http://pie.com/"));
  std::vector<base::Time> new_visits1;
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_NONE,
            TypedUrlModelAssociator::MergeUrls(specs1, row1, &visits1,
                                               &new_row1, &new_visits1));

  history::VisitVector visits2;
  history::URLRow row2(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 3, false, &visits2));
  sync_pb::TypedUrlSpecifics specs2(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          2, 3, true));
  history::VisitVector expected_visits2;
  history::URLRow expected2(MakeTypedUrlRow("http://pie.com/", "pie",
                                            2, 3, true, &expected_visits2));
  history::URLRow new_row2(GURL("http://pie.com/"));
  std::vector<base::Time> new_visits2;
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_ROW_CHANGED,
            TypedUrlModelAssociator::MergeUrls(specs2, row2, &visits2,
                                               &new_row2, &new_visits2));
  EXPECT_TRUE(URLsEqual(new_row2, expected2));

  history::VisitVector visits3;
  history::URLRow row3(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 3, false, &visits3));
  sync_pb::TypedUrlSpecifics specs3(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie2",
                                                          2, 3, true));
  history::VisitVector expected_visits3;
  history::URLRow expected3(MakeTypedUrlRow("http://pie.com/", "pie2",
                                            2, 3, true, &expected_visits3));
  history::URLRow new_row3(GURL("http://pie.com/"));
  std::vector<base::Time> new_visits3;
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_ROW_CHANGED |
            TypedUrlModelAssociator::DIFF_TITLE_CHANGED,
            TypedUrlModelAssociator::MergeUrls(specs3, row3, &visits3,
                                               &new_row3, &new_visits3));
  EXPECT_TRUE(URLsEqual(new_row3, expected3));

  history::VisitVector visits4;
  history::URLRow row4(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 4, false, &visits4));
  sync_pb::TypedUrlSpecifics specs4(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie2",
                                                          2, 3, true));
  history::VisitVector expected_visits4;
  history::URLRow expected4(MakeTypedUrlRow("http://pie.com/", "pie",
                                            2, 3, false, &expected_visits4));
  history::URLRow new_row4(GURL("http://pie.com/"));
  std::vector<base::Time> new_visits4;
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_NODE_CHANGED |
            TypedUrlModelAssociator::DIFF_VISITS_ADDED,
            TypedUrlModelAssociator::MergeUrls(specs4, row4, &visits4,
                                               &new_row4, &new_visits4));
  EXPECT_TRUE(URLsEqual(new_row4, expected4));

  history::VisitVector visits5;
  history::URLRow row5(MakeTypedUrlRow("http://pie.com/", "pie",
                                       1, 4, false, &visits5));
  sync_pb::TypedUrlSpecifics specs5(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          2, 3, false));
  history::VisitVector expected_visits5;
  history::URLRow expected5(MakeTypedUrlRow("http://pie.com/", "pie",
                                            2, 3, false, &expected_visits5));
  history::URLRow new_row5(GURL("http://pie.com/"));
  std::vector<base::Time> new_visits5;
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_ROW_CHANGED |
            TypedUrlModelAssociator::DIFF_VISITS_ADDED,
            TypedUrlModelAssociator::MergeUrls(specs5, row5, &visits5,
                                               &new_row5, &new_visits5));
  EXPECT_TRUE(URLsEqual(new_row5, expected5));

}

TEST_F(TypedUrlModelAssociatorTest, DiffVisitsSame) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64 visits[] = { 1024, 2065, 65534, 1237684 };

  for (size_t c = 0; c < arraysize(visits); ++c) {
    old_visits.push_back(history::VisitRow(
        0, base::Time::FromInternalValue(visits[c]), 0, 0, 0));
    new_url.add_visit(visits[c]);
  }

  std::vector<base::Time> new_visits;
  history::VisitVector removed_visits;

  TypedUrlModelAssociator::DiffVisits(old_visits, new_url,
                                      &new_visits, &removed_visits);
  EXPECT_TRUE(new_visits.empty());
  EXPECT_TRUE(removed_visits.empty());
}

TEST_F(TypedUrlModelAssociatorTest, DiffVisitsRemove) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64 visits_left[] = { 1, 1024, 1500, 2065, 6000,
                                65534, 1237684, 2237684 };
  const int64 visits_right[] = { 1024, 2065, 65534, 1237684 };

  const int64 visits_removed[] = { 1, 1500, 6000, 2237684 };

  for (size_t c = 0; c < arraysize(visits_left); ++c) {
    old_visits.push_back(history::VisitRow(
        0, base::Time::FromInternalValue(visits_left[c]), 0, 0, 0));
  }

  for (size_t c = 0; c < arraysize(visits_right); ++c) {
    new_url.add_visit(visits_right[c]);
  }

  std::vector<base::Time> new_visits;
  history::VisitVector removed_visits;

  TypedUrlModelAssociator::DiffVisits(old_visits, new_url,
                                      &new_visits, &removed_visits);
  EXPECT_TRUE(new_visits.empty());
  ASSERT_TRUE(removed_visits.size() == arraysize(visits_removed));
  for (size_t c = 0; c < arraysize(visits_removed); ++c) {
    EXPECT_EQ(removed_visits[c].visit_time.ToInternalValue(),
              visits_removed[c]);
  }
}

TEST_F(TypedUrlModelAssociatorTest, DiffVisitsAdd) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64 visits_left[] = { 1024, 2065, 65534, 1237684 };
  const int64 visits_right[] = { 1, 1024, 1500, 2065, 6000,
                                65534, 1237684, 2237684 };

  const int64 visits_added[] = { 1, 1500, 6000, 2237684 };

  for (size_t c = 0; c < arraysize(visits_left); ++c) {
    old_visits.push_back(history::VisitRow(
        0, base::Time::FromInternalValue(visits_left[c]), 0, 0, 0));
  }

  for (size_t c = 0; c < arraysize(visits_right); ++c) {
    new_url.add_visit(visits_right[c]);
  }

  std::vector<base::Time> new_visits;
  history::VisitVector removed_visits;

  TypedUrlModelAssociator::DiffVisits(old_visits, new_url,
                                      &new_visits, &removed_visits);
  EXPECT_TRUE(removed_visits.empty());
  ASSERT_TRUE(new_visits.size() == arraysize(visits_added));
  for (size_t c = 0; c < arraysize(visits_added); ++c) {
    EXPECT_EQ(new_visits[c].ToInternalValue(),
              visits_added[c]);
  }
}

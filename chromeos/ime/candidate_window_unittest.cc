// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more tests.

#include "chromeos/ime/candidate_window.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

TEST(CandidateWindow, IsEqualTest) {
  CandidateWindow cw1;
  CandidateWindow cw2;

  const char kSampleString1[] = "Sample 1";
  const char kSampleString2[] = "Sample 2";

  EXPECT_TRUE(cw1.IsEqual(cw2));
  EXPECT_TRUE(cw2.IsEqual(cw1));

  cw1.set_page_size(1);
  cw2.set_page_size(2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_page_size(1);

  cw1.set_cursor_position(1);
  cw2.set_cursor_position(2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_cursor_position(1);

  cw1.set_is_cursor_visible(true);
  cw2.set_is_cursor_visible(false);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_is_cursor_visible(true);

  cw1.set_orientation(CandidateWindow::HORIZONTAL);
  cw2.set_orientation(CandidateWindow::VERTICAL);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_orientation(CandidateWindow::HORIZONTAL);

  cw1.set_show_window_at_composition(true);
  cw2.set_show_window_at_composition(false);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_show_window_at_composition(true);

  // Check equality for candidates member variable.
  CandidateWindow::Entry entry1;
  CandidateWindow::Entry entry2;

  cw1.mutable_candidates()->push_back(entry1);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_TRUE(cw1.IsEqual(cw2));
  EXPECT_TRUE(cw2.IsEqual(cw1));

  entry1.value = kSampleString1;
  entry2.value = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();

  entry1.label = kSampleString1;
  entry2.label = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();

  entry1.annotation = kSampleString1;
  entry2.annotation = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();

  entry1.description_title = kSampleString1;
  entry2.description_title = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();

  entry1.description_body = kSampleString1;
  entry2.description_body = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();
}

TEST(CandidateWindow, CopyFromTest) {
  CandidateWindow cw1;
  CandidateWindow cw2;

  const char kSampleString[] = "Sample";

  cw1.set_page_size(1);
  cw1.set_cursor_position(2);
  cw1.set_is_cursor_visible(false);
  cw1.set_orientation(CandidateWindow::HORIZONTAL);
  cw1.set_show_window_at_composition(false);

  CandidateWindow::Entry entry;
  entry.value = kSampleString;
  entry.label = kSampleString;
  entry.annotation = kSampleString;
  entry.description_title = kSampleString;
  entry.description_body = kSampleString;
  cw1.mutable_candidates()->push_back(entry);

  cw2.CopyFrom(cw1);
  EXPECT_TRUE(cw1.IsEqual(cw2));
}

}  // namespace input_method
}  // namespace chromeos

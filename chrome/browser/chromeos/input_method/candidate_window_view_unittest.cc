// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/candidate_window_view.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/candidate_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

namespace {
const char* kSampleCandidate[] = {
  "Sample Candidate 1",
  "Sample Candidate 2",
  "Sample Candidate 3"
};
const char* kSampleLabel[] = {
  "Sample Label 1",
  "Sample Label 2",
  "Sample Label 3"
};
const char* kSampleAnnotation[] = {
  "Sample Annotation 1",
  "Sample Annotation 2",
  "Sample Annotation 3"
};
const char* kSampleDescriptionTitle[] = {
  "Sample Description Title 1",
  "Sample Description Title 2",
  "Sample Description Title 3",
};
const char* kSampleDescriptionBody[] = {
  "Sample Description Body 1",
  "Sample Description Body 2",
  "Sample Description Body 3",
};

void ClearInputMethodLookupTable(size_t page_size,
                                 InputMethodLookupTable* table) {
  table->visible = false;
  table->cursor_absolute_index = 0;
  table->page_size = page_size;
  table->candidates.clear();
  table->orientation = InputMethodLookupTable::kVertical;
  table->labels.clear();
  table->annotations.clear();
}

}  // namespace

class CandidateWindowViewTest : public views::ViewsTestBase {
 protected:
  void ExpectLabels(const std::string shortcut,
                    const std::string candidate,
                    const std::string annotation,
                    const CandidateView* row) {
    EXPECT_EQ(shortcut, UTF16ToUTF8(row->shortcut_label_->text()));
    EXPECT_EQ(candidate, UTF16ToUTF8(row->candidate_label_->text()));
    EXPECT_EQ(annotation, UTF16ToUTF8(row->annotation_label_->text()));
  }
};

TEST_F(CandidateWindowViewTest, ShouldUpdateCandidateViewsTest) {
  // This test verifies the process of judging update lookup-table or not.
  // This judgement is handled by ShouldUpdateCandidateViews, which returns true
  // if update is necessary and vice versa.
  InputMethodLookupTable old_table;
  InputMethodLookupTable new_table;

  const size_t kPageSize = 10;

  ClearInputMethodLookupTable(kPageSize, &old_table);
  ClearInputMethodLookupTable(kPageSize, &new_table);

  old_table.visible = true;
  old_table.cursor_absolute_index = 0;
  old_table.page_size = 1;
  old_table.candidates.clear();
  old_table.orientation = InputMethodLookupTable::kVertical;
  old_table.labels.clear();
  old_table.annotations.clear();

  new_table = old_table;

  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));

  new_table.visible = false;
  // Visibility would be ignored.
  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));
  new_table = old_table;
  new_table.candidates.push_back(kSampleCandidate[0]);
  old_table.candidates.push_back(kSampleCandidate[0]);
  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));
  new_table.labels.push_back(kSampleLabel[0]);
  old_table.labels.push_back(kSampleLabel[0]);
  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));
  new_table.annotations.push_back(kSampleAnnotation[0]);
  old_table.annotations.push_back(kSampleAnnotation[0]);
  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));

  InputMethodLookupTable::Description description;
  description.title = kSampleDescriptionTitle[0];
  description.body = kSampleDescriptionBody[0];

  new_table.descriptions.push_back(description);
  old_table.descriptions.push_back(description);
  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));

  new_table.cursor_absolute_index = 1;
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table = old_table;

  new_table.page_size = 2;
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table = old_table;

  new_table.orientation = InputMethodLookupTable::kHorizontal;
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  new_table = old_table;
  new_table.candidates.push_back(kSampleCandidate[1]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  old_table.candidates.push_back(kSampleCandidate[2]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.candidates.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.candidates.push_back(kSampleCandidate[1]);
  old_table.candidates.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  new_table = old_table;
  new_table.labels.push_back(kSampleLabel[1]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  old_table.labels.push_back(kSampleLabel[2]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.labels.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.labels.push_back(kSampleLabel[1]);
  old_table.labels.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  new_table = old_table;
  new_table.annotations.push_back(kSampleAnnotation[1]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  old_table.annotations.push_back(kSampleAnnotation[2]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.annotations.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.annotations.push_back(kSampleAnnotation[1]);
  old_table.annotations.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  new_table = old_table;
  new_table.annotations.push_back(kSampleDescriptionTitle[1]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  old_table.annotations.push_back(kSampleDescriptionTitle[2]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.annotations.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.annotations.push_back(kSampleDescriptionTitle[1]);
  old_table.annotations.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  new_table = old_table;
  new_table.annotations.push_back(kSampleDescriptionBody[1]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  old_table.annotations.push_back(kSampleDescriptionBody[2]);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.annotations.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.annotations.push_back(kSampleDescriptionBody[1]);
  old_table.annotations.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
}

TEST_F(CandidateWindowViewTest, ShortcutSettingTest) {
  const char* kEmptyLabel = "";
  const char* kCustomizedLabel[] = { "a", "s", "d" };
  const char* kExpectedHorizontalCustomizedLabel[] = { "a.", "s.", "d." };

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  widget->Init(params);

  CandidateWindowView candidate_window_view(widget);
  candidate_window_view.Init();

  {
    SCOPED_TRACE("candidate_views allocation test");
    const size_t kMaxPageSize = 16;
    for (size_t i = 1; i < kMaxPageSize; ++i) {
      InputMethodLookupTable table;
      ClearInputMethodLookupTable(i, &table);
      candidate_window_view.UpdateCandidates(table);
      EXPECT_EQ(i, candidate_window_view.candidate_views_.size());
    }
  }
  {
    SCOPED_TRACE("Empty string for each labels expects empty labels(vertical)");
    const size_t kPageSize = 3;
    InputMethodLookupTable table;
    ClearInputMethodLookupTable(kPageSize, &table);

    table.orientation = InputMethodLookupTable::kVertical;
    for (size_t i = 0; i < kPageSize; ++i) {
      table.candidates.push_back(kSampleCandidate[i]);
      table.annotations.push_back(kSampleAnnotation[i]);
      InputMethodLookupTable::Description description;
      description.title = kSampleDescriptionTitle[i];
      description.body = kSampleDescriptionBody[i];
      table.descriptions.push_back(description);
      table.labels.push_back(kEmptyLabel);
    }

    candidate_window_view.UpdateCandidates(table);

    ASSERT_EQ(kPageSize, candidate_window_view.candidate_views_.size());
    for (size_t i = 0; i < kPageSize; ++i) {
      ExpectLabels(kEmptyLabel, kSampleCandidate[i], kSampleAnnotation[i],
                   candidate_window_view.candidate_views_[i]);
    }
  }
  {
    SCOPED_TRACE(
        "Empty string for each labels expect empty labels(horizontal)");
    const size_t kPageSize = 3;
    InputMethodLookupTable table;
    ClearInputMethodLookupTable(kPageSize, &table);

    table.orientation = InputMethodLookupTable::kHorizontal;
    for (size_t i = 0; i < kPageSize; ++i) {
      table.candidates.push_back(kSampleCandidate[i]);
      table.annotations.push_back(kSampleAnnotation[i]);
      InputMethodLookupTable::Description description;
      description.title = kSampleDescriptionTitle[i];
      description.body = kSampleDescriptionBody[i];
      table.descriptions.push_back(description);
      table.labels.push_back(kEmptyLabel);
    }

    candidate_window_view.UpdateCandidates(table);

    ASSERT_EQ(kPageSize, candidate_window_view.candidate_views_.size());
    // Confirm actual labels not containing ".".
    for (size_t i = 0; i < kPageSize; ++i) {
      ExpectLabels(kEmptyLabel, kSampleCandidate[i], kSampleAnnotation[i],
                   candidate_window_view.candidate_views_[i]);
    }
  }
  {
    SCOPED_TRACE("Vertical customized label case");
    const size_t kPageSize = 3;
    InputMethodLookupTable table;
    ClearInputMethodLookupTable(kPageSize, &table);

    table.orientation = InputMethodLookupTable::kVertical;
    for (size_t i = 0; i < kPageSize; ++i) {
      table.candidates.push_back(kSampleCandidate[i]);
      table.annotations.push_back(kSampleAnnotation[i]);
      InputMethodLookupTable::Description description;
      description.title = kSampleDescriptionTitle[i];
      description.body = kSampleDescriptionBody[i];
      table.descriptions.push_back(description);
      table.labels.push_back(kCustomizedLabel[i]);
    }

    candidate_window_view.UpdateCandidates(table);

    ASSERT_EQ(kPageSize, candidate_window_view.candidate_views_.size());
    // Confirm actual labels not containing ".".
    for (size_t i = 0; i < kPageSize; ++i) {
      ExpectLabels(kCustomizedLabel[i],
                   kSampleCandidate[i],
                   kSampleAnnotation[i],
                   candidate_window_view.candidate_views_[i]);
    }
  }
  {
    SCOPED_TRACE("Horizontal customized label case");
    const size_t kPageSize = 3;
    InputMethodLookupTable table;
    ClearInputMethodLookupTable(kPageSize, &table);

    table.orientation = InputMethodLookupTable::kHorizontal;
    for (size_t i = 0; i < kPageSize; ++i) {
      table.candidates.push_back(kSampleCandidate[i]);
      table.annotations.push_back(kSampleAnnotation[i]);
      InputMethodLookupTable::Description description;
      description.title = kSampleDescriptionTitle[i];
      description.body = kSampleDescriptionBody[i];
      table.descriptions.push_back(description);
      table.labels.push_back(kCustomizedLabel[i]);
    }

    candidate_window_view.UpdateCandidates(table);

    ASSERT_EQ(kPageSize, candidate_window_view.candidate_views_.size());
    // Confirm actual labels not containing ".".
    for (size_t i = 0; i < kPageSize; ++i) {
      ExpectLabels(kExpectedHorizontalCustomizedLabel[i],
                   kSampleCandidate[i],
                   kSampleAnnotation[i],
                   candidate_window_view.candidate_views_[i]);
    }
  }

  // We should call CloseNow method, otherwise this test will leak memory.
  widget->CloseNow();
}

TEST_F(CandidateWindowViewTest, DoNotChangeRowHeightWithLabelSwitchTest) {
  const size_t kPageSize = 10;
  InputMethodLookupTable table;
  InputMethodLookupTable no_shortcut_table;

  const char kSampleCandidate1[] = "Sample String 1";
  const char kSampleCandidate2[] = "\xE3\x81\x82";  // multi byte string.
  const char kSampleCandidate3[] = ".....";

  const char kSampleShortcut1[] = "1";
  const char kSampleShortcut2[] = "b";
  const char kSampleShortcut3[] = "C";

  const char kSampleAnnotation1[] = "Sample Annotation 1";
  const char kSampleAnnotation2[] = "\xE3\x81\x82";  // multi byte string.
  const char kSampleAnnotation3[] = "......";

  // For testing, we have to prepare empty widget.
  // We should NOT manually free widget by default, otherwise double free will
  // be occurred. So, we should instantiate widget class with "new" operation.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  widget->Init(params);

  CandidateWindowView candidate_window_view(widget);
  candidate_window_view.Init();

  // Create LookupTable object.
  ClearInputMethodLookupTable(kPageSize, &table);
  table.visible = true;
  table.cursor_absolute_index = 0;
  table.page_size = 3;
  table.candidates.clear();
  table.orientation = InputMethodLookupTable::kVertical;
  table.labels.clear();
  table.annotations.clear();

  table.candidates.push_back(kSampleCandidate1);
  table.candidates.push_back(kSampleCandidate2);
  table.candidates.push_back(kSampleCandidate3);

  table.labels.push_back(kSampleShortcut1);
  table.labels.push_back(kSampleShortcut2);
  table.labels.push_back(kSampleShortcut3);

  table.annotations.push_back(kSampleAnnotation1);
  table.annotations.push_back(kSampleAnnotation2);
  table.annotations.push_back(kSampleAnnotation3);

  no_shortcut_table = table;
  no_shortcut_table.labels.clear();

  int before_height = 0;

  // Test for shortcut mode to no-shortcut mode.
  // Initialize with a shortcut mode lookup table.
  candidate_window_view.MaybeInitializeCandidateViews(table);
  ASSERT_EQ(3UL, candidate_window_view.candidate_views_.size());
  before_height =
      candidate_window_view.candidate_views_[0]->GetContentsBounds().height();
  // Checks all entry have same row height.
  for (size_t i = 1; i < candidate_window_view.candidate_views_.size(); ++i) {
    const CandidateView* view = candidate_window_view.candidate_views_[i];
    EXPECT_EQ(before_height, view->GetContentsBounds().height());
  }

  // Initialize with a no shortcut mode lookup table.
  candidate_window_view.MaybeInitializeCandidateViews(no_shortcut_table);
  ASSERT_EQ(3UL, candidate_window_view.candidate_views_.size());
  EXPECT_EQ(before_height,
            candidate_window_view.candidate_views_[0]->GetContentsBounds()
                .height());
  // Checks all entry have same row height.
  for (size_t i = 1; i < candidate_window_view.candidate_views_.size(); ++i) {
    const CandidateView* view = candidate_window_view.candidate_views_[i];
    EXPECT_EQ(before_height, view->GetContentsBounds().height());
  }

  // Test for no-shortcut mode to shortcut mode.
  // Initialize with a no shortcut mode lookup table.
  candidate_window_view.MaybeInitializeCandidateViews(no_shortcut_table);
  ASSERT_EQ(3UL, candidate_window_view.candidate_views_.size());
  before_height =
      candidate_window_view.candidate_views_[0]->GetContentsBounds().height();
  // Checks all entry have same row height.
  for (size_t i = 1; i < candidate_window_view.candidate_views_.size(); ++i) {
    const CandidateView* view = candidate_window_view.candidate_views_[i];
    EXPECT_EQ(before_height, view->GetContentsBounds().height());
  }

  // Initialize with a shortcut mode lookup table.
  candidate_window_view.MaybeInitializeCandidateViews(table);
  ASSERT_EQ(3UL, candidate_window_view.candidate_views_.size());
  EXPECT_EQ(before_height,
            candidate_window_view.candidate_views_[0]->GetContentsBounds()
                .height());
  // Checks all entry have same row height.
  for (size_t i = 1; i < candidate_window_view.candidate_views_.size(); ++i) {
    const CandidateView* view = candidate_window_view.candidate_views_[i];
    EXPECT_EQ(before_height, view->GetContentsBounds().height());
  }

  // We should call CloseNow method, otherwise this test will leak memory.
  widget->CloseNow();
}
}  // namespace input_method
}  // namespace chromeos

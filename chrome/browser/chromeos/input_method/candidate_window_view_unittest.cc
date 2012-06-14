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

void ClearInputMethodLookupTable(size_t page_size,
                                 InputMethodLookupTable* table) {
  table->visible = false;
  table->cursor_absolute_index = 0;
  table->page_size = page_size;
  table->candidates.clear();
  table->orientation = InputMethodLookupTable::kVertical;
  table->labels.clear();
  table->annotations.clear();
  table->mozc_candidates.Clear();
}

void InitializeMozcCandidates(InputMethodLookupTable* table) {
  table->mozc_candidates.Clear();
  table->mozc_candidates.set_position(0);
  table->mozc_candidates.set_size(0);
}

void AppendCandidateIntoLookupTable(InputMethodLookupTable* table,
                                    const std::string& value) {
  mozc::commands::Candidates::Candidate *candidate =
      table->mozc_candidates.add_candidate();

  int current_entry_count = table->mozc_candidates.candidate_size();
  table->candidates.push_back(value);
  candidate->set_index(current_entry_count);
  candidate->set_value(value);
  candidate->set_id(current_entry_count);
  candidate->set_information_id(current_entry_count);
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
  const char* kSampleCandidate1 = "Sample Candidate 1";
  const char* kSampleCandidate2 = "Sample Candidate 2";
  const char* kSampleCandidate3 = "Sample Candidate 3";

  const char* kSampleAnnotation1 = "Sample Annotation 1";
  const char* kSampleAnnotation2 = "Sample Annotation 2";
  const char* kSampleAnnotation3 = "Sample Annotation 3";

  const char* kSampleLabel1 = "Sample Label 1";
  const char* kSampleLabel2 = "Sample Label 2";
  const char* kSampleLabel3 = "Sample Label 3";

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
  new_table.candidates.push_back(kSampleCandidate1);
  old_table.candidates.push_back(kSampleCandidate1);
  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));
  new_table.labels.push_back(kSampleLabel1);
  old_table.labels.push_back(kSampleLabel1);
  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));
  new_table.annotations.push_back(kSampleAnnotation1);
  old_table.annotations.push_back(kSampleAnnotation1);
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
  new_table.candidates.push_back(kSampleCandidate2);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  old_table.candidates.push_back(kSampleCandidate3);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.candidates.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.candidates.push_back(kSampleCandidate2);
  old_table.candidates.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  new_table = old_table;
  new_table.labels.push_back(kSampleLabel2);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  old_table.labels.push_back(kSampleLabel3);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.labels.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.labels.push_back(kSampleLabel2);
  old_table.labels.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  new_table = old_table;
  new_table.annotations.push_back(kSampleAnnotation2);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  old_table.annotations.push_back(kSampleAnnotation3);
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.annotations.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
  new_table.annotations.push_back(kSampleAnnotation2);
  old_table.annotations.clear();
  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
}

TEST_F(CandidateWindowViewTest, MozcSuggestWindowShouldUpdateTest) {
  // ShouldUpdateCandidateViews method should also judge with consideration of
  // the mozc specific candidate information. Following tests verify them.
  const char* kSampleCandidate1 = "Sample Candidate 1";
  const char* kSampleCandidate2 = "Sample Candidate 2";

  const size_t kPageSize = 10;

  InputMethodLookupTable old_table;
  InputMethodLookupTable new_table;

  // State chagne from using non-mozc candidate to mozc candidate.
  ClearInputMethodLookupTable(kPageSize, &old_table);
  ClearInputMethodLookupTable(kPageSize, &new_table);

  old_table.candidates.push_back(kSampleCandidate1);
  InitializeMozcCandidates(&new_table);
  AppendCandidateIntoLookupTable(&new_table, kSampleCandidate2);

  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  // State change from using mozc candidate to non-mozc candidate
  ClearInputMethodLookupTable(kPageSize, &old_table);
  ClearInputMethodLookupTable(kPageSize, &new_table);

  InitializeMozcCandidates(&old_table);
  AppendCandidateIntoLookupTable(&old_table, kSampleCandidate1);

  new_table.candidates.push_back(kSampleCandidate2);

  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));

  // State change from using mozc candidate to mozc candidate

  // No change
  ClearInputMethodLookupTable(kPageSize, &old_table);
  ClearInputMethodLookupTable(kPageSize, &new_table);

  InitializeMozcCandidates(&old_table);
  AppendCandidateIntoLookupTable(&old_table, kSampleCandidate1);

  InitializeMozcCandidates(&new_table);
  AppendCandidateIntoLookupTable(&new_table, kSampleCandidate1);

  EXPECT_FALSE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                               new_table));
  // Candidate contents
  ClearInputMethodLookupTable(kPageSize, &old_table);
  ClearInputMethodLookupTable(kPageSize, &new_table);

  InitializeMozcCandidates(&old_table);
  AppendCandidateIntoLookupTable(&old_table, kSampleCandidate1);

  InitializeMozcCandidates(&new_table);
  AppendCandidateIntoLookupTable(&new_table, kSampleCandidate2);

  EXPECT_TRUE(CandidateWindowView::ShouldUpdateCandidateViews(old_table,
                                                              new_table));
}

TEST_F(CandidateWindowViewTest, MozcUpdateCandidateTest) {
  // This test verifies whether UpdateCandidates function updates window mozc
  // specific candidate position correctly on the correct condition.

  // For testing, we have to prepare empty widget.
  // We should NOT manually free widget by default, otherwise double free will
  // be occurred. So, we should instantiate widget class with "new" operation.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  widget->Init(params);

  CandidateWindowView candidate_window_view(widget);
  candidate_window_view.Init();

  const size_t kPageSize = 10;

  InputMethodLookupTable new_table;
  ClearInputMethodLookupTable(kPageSize, &new_table);
  InitializeMozcCandidates(&new_table);

  // If candidate category is SUGGESTION, should not show at composition head.
  new_table.mozc_candidates.set_category(mozc::commands::CONVERSION);
  candidate_window_view.UpdateCandidates(new_table);
  EXPECT_FALSE(candidate_window_view.should_show_at_composition_head_);

  // If candidate category is SUGGESTION, should show at composition head.
  new_table.mozc_candidates.set_category(mozc::commands::SUGGESTION);
  candidate_window_view.UpdateCandidates(new_table);
  EXPECT_TRUE(candidate_window_view.should_show_at_composition_head_);

  // We should call CloseNow method, otherwise this test will leak memory.
  widget->CloseNow();
}

TEST_F(CandidateWindowViewTest, ShortcutSettingTest) {
  const char* kSampleCandidate[] = {
    "Sample Candidate 1",
    "Sample Candidate 2",
    "Sample Candidate 3"
  };
  const char* kSampleAnnotation[] = {
    "Sample Annotation 1",
    "Sample Annotation 2",
    "Sample Annotation 3"
  };
  const char* kEmptyLabel = "";
  const char* kDefaultVerticalLabel[] = { "1", "2", "3" };
  const char* kDefaultHorizontalLabel[] = { "1.", "2.", "3." };

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
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
    SCOPED_TRACE("Empty labels expects default label(vertical)");
    const size_t kPageSize = 3;
    InputMethodLookupTable table;
    ClearInputMethodLookupTable(kPageSize, &table);

    table.orientation = InputMethodLookupTable::kVertical;
    for (size_t i = 0; i < kPageSize; ++i) {
      table.candidates.push_back(kSampleCandidate[i]);
      table.annotations.push_back(kSampleAnnotation[i]);
    }

    table.labels.clear();

    candidate_window_view.UpdateCandidates(table);

    ASSERT_EQ(kPageSize, candidate_window_view.candidate_views_.size());
    for (size_t i = 0; i < kPageSize; ++i) {
      ExpectLabels(kDefaultVerticalLabel[i],
                   kSampleCandidate[i],
                   kSampleAnnotation[i],
                   candidate_window_view.candidate_views_[i]);
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
    SCOPED_TRACE("Empty labels expects default label(horizontal)");
    const size_t kPageSize = 3;
    InputMethodLookupTable table;
    ClearInputMethodLookupTable(kPageSize, &table);

    table.orientation = InputMethodLookupTable::kHorizontal;
    for (size_t i = 0; i < kPageSize; ++i) {
      table.candidates.push_back(kSampleCandidate[i]);
      table.annotations.push_back(kSampleAnnotation[i]);
    }

    table.labels.clear();

    candidate_window_view.UpdateCandidates(table);

    ASSERT_EQ(kPageSize, candidate_window_view.candidate_views_.size());
    for (size_t i = 0; i < kPageSize; ++i) {
      ExpectLabels(kDefaultHorizontalLabel[i],
                   kSampleCandidate[i],
                   kSampleAnnotation[i],
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

  // We should call CloseNow method, otherwise this test will leak memory.
  widget->CloseNow();
}

class InfolistWindowViewTest : public views::ViewsTestBase {
};

TEST_F(InfolistWindowViewTest, ShouldUpdateViewTest) {
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    mozc::commands::Information *new_info = new_usages.add_information();
    new_info->set_title("title1");
    new_info->set_description("description1");
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    mozc::commands::Information *new_info = new_usages.add_information();
    new_info->set_title("title2");
    new_info->set_description("description1");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    mozc::commands::Information *new_info = new_usages.add_information();
    new_info->set_title("title1");
    new_info->set_description("description2");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info = old_usages.add_information();
    old_info->set_title("title1");
    old_info->set_description("description1");
    mozc::commands::Information *new_info = new_usages.add_information();
    new_info->set_title("title2");
    new_info->set_description("description2");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info1 = old_usages.add_information();
    old_info1->set_title("title1");
    old_info1->set_description("description1");
    mozc::commands::Information *old_info2 = old_usages.add_information();
    old_info2->set_title("title2");
    old_info2->set_description("description2");
    mozc::commands::Information *new_info1 = new_usages.add_information();
    new_info1->set_title("title1");
    new_info1->set_description("description1");
    mozc::commands::Information *new_info2 = new_usages.add_information();
    new_info2->set_title("title2");
    new_info2->set_description("description2");
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
    old_usages.set_focused_index(0);
    new_usages.set_focused_index(0);
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
    old_usages.set_focused_index(0);
    new_usages.set_focused_index(1);
    EXPECT_FALSE(InfolistWindowView::ShouldUpdateView(&old_usages,
                                                      &new_usages));
  }
  {
    mozc::commands::InformationList old_usages;
    mozc::commands::InformationList new_usages;
    mozc::commands::Information *old_info1 = old_usages.add_information();
    old_info1->set_title("title1");
    old_info1->set_description("description1");
    mozc::commands::Information *old_info2 = old_usages.add_information();
    old_info2->set_title("title2");
    old_info2->set_description("description2");
    mozc::commands::Information *new_info1 = new_usages.add_information();
    new_info1->set_title("title1");
    new_info1->set_description("description1");
    mozc::commands::Information *new_info2 = new_usages.add_information();
    new_info2->set_title("title3");
    new_info2->set_description("description3");
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
    old_usages.set_focused_index(0);
    new_usages.set_focused_index(0);
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
    old_usages.set_focused_index(0);
    new_usages.set_focused_index(1);
    EXPECT_TRUE(InfolistWindowView::ShouldUpdateView(&old_usages, &new_usages));
  }
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
  const char kSampleAnnotation2[] = "\xE3\x81\x82"; // multi byte string.
  const char kSampleAnnotation3[] = "......";

  // For testing, we have to prepare empty widget.
  // We should NOT manually free widget by default, otherwise double free will
  // be occurred. So, we should instantiate widget class with "new" operation.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
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

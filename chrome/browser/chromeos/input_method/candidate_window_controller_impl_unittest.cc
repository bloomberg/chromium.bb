// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/candidate_window_controller_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {

const size_t kSampleCandidateSize = 3;
const char* kSampleCandidate[] = {
  "Sample Candidate 1",
  "Sample Candidate 2",
  "Sample Candidate 3",
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

class TestableCandidateWindowControllerImpl :
    public CandidateWindowControllerImpl {
 public:
  TestableCandidateWindowControllerImpl() {}
  virtual ~TestableCandidateWindowControllerImpl() {}

  // Changes access right for testing.
  using CandidateWindowControllerImpl::GetInfolistWindowPosition;
  using CandidateWindowControllerImpl::ConvertLookupTableToInfolistEntry;
  using CandidateWindowControllerImpl::ShouldUpdateInfolist;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestableCandidateWindowControllerImpl);
};

}  // namespace

class CandidateWindowControllerImplTest : public testing::Test {
 public:
  CandidateWindowControllerImplTest()
      : kScreenRect(gfx::Rect(0, 0, 1000, 1000)) {
  }

  virtual ~CandidateWindowControllerImplTest() {
  }

 protected:
  const gfx::Rect kScreenRect;
};

TEST_F(CandidateWindowControllerImplTest,
       GetInfolistWindowPositionTest_NoOverflow) {
  const gfx::Rect candidate_window_rect(100, 110, 120, 130);
  const gfx::Size infolist_window_size(200, 200);

  // If there is no overflow to show infolist window, the expected position is
  // right-top of candidate window position.
  const gfx::Point expected_point(candidate_window_rect.right(),
                                  candidate_window_rect.y());
  EXPECT_EQ(expected_point,
            TestableCandidateWindowControllerImpl::GetInfolistWindowPosition(
                candidate_window_rect,
                kScreenRect,
                infolist_window_size));
}

TEST_F(CandidateWindowControllerImplTest,
       GetInfolistWindowPositionTest_RightOverflow) {
  const gfx::Rect candidate_window_rect(900, 110, 120, 130);
  const gfx::Size infolist_window_size(200, 200);

  // If right of infolist window area is no in screen, show infolist left side
  // of candidate window, that is
  // x : candidate_window_rect.left - infolist_window.width,
  // y : candidate_window_rect.top
  const gfx::Point expected_point(
      candidate_window_rect.x() - infolist_window_size.width(),
      candidate_window_rect.y());
  EXPECT_EQ(expected_point,
            TestableCandidateWindowControllerImpl::GetInfolistWindowPosition(
                candidate_window_rect,
                kScreenRect,
                infolist_window_size));
}

TEST_F(CandidateWindowControllerImplTest,
       GetInfolistWindowPositionTest_BottomOverflow) {
  const gfx::Rect candidate_window_rect(100, 910, 120, 130);
  const gfx::Size infolist_window_size(200, 200);

  // If bottom of infolist window area is no in screen, show infolist clipping
  // with screen bounds.
  // x : candidate_window_rect.right
  // y : screen_rect.bottom - infolist_window_rect.height
  const gfx::Point expected_point(
      candidate_window_rect.right(),
      kScreenRect.bottom() - infolist_window_size.height());
  EXPECT_EQ(expected_point,
            TestableCandidateWindowControllerImpl::GetInfolistWindowPosition(
                candidate_window_rect,
                kScreenRect,
                infolist_window_size));
}

TEST_F(CandidateWindowControllerImplTest,
       GetInfolistWindowPositionTest_RightBottomOverflow) {
  const gfx::Rect candidate_window_rect(900, 910, 120, 130);
  const gfx::Size infolist_window_size(200, 200);

  const gfx::Point expected_point(
      candidate_window_rect.x() - infolist_window_size.width(),
      kScreenRect.bottom() - infolist_window_size.height());
  EXPECT_EQ(expected_point,
            TestableCandidateWindowControllerImpl::GetInfolistWindowPosition(
                candidate_window_rect,
                kScreenRect,
                infolist_window_size));
}

TEST_F(CandidateWindowControllerImplTest,
       ConvertLookupTableToInfolistEntryTest_DenseCase) {
  ibus::IBusLookupTable table;
  table.set_page_size(10);
  for (size_t i = 0; i < kSampleCandidateSize; ++i) {
    ibus::IBusLookupTable::Entry entry;
    entry.value = kSampleCandidate[i];
    entry.description_title = kSampleDescriptionTitle[i];
    entry.description_body = kSampleDescriptionBody[i];
    table.mutable_candidates()->push_back(entry);
  }
  table.set_cursor_position(1);

  std::vector<InfolistWindowView::Entry> infolist_entries;
  size_t focused_index = 0;

  TestableCandidateWindowControllerImpl::ConvertLookupTableToInfolistEntry(
      table,
      &infolist_entries,
      &focused_index);

  EXPECT_EQ(kSampleCandidateSize, infolist_entries.size());
  EXPECT_EQ(1UL, focused_index);
}

TEST_F(CandidateWindowControllerImplTest,
       ConvertLookupTableToInfolistEntryTest_SparseCase) {
  ibus::IBusLookupTable table;
  table.set_page_size(10);
  for (size_t i = 0; i < kSampleCandidateSize; ++i) {
    ibus::IBusLookupTable::Entry entry;
    entry.value = kSampleCandidate[i];
    table.mutable_candidates()->push_back(entry);
  }

  std::vector<ibus::IBusLookupTable::Entry>* candidates =
      table.mutable_candidates();
  (*candidates)[2].description_title = kSampleDescriptionTitle[2];
  (*candidates)[2].description_body = kSampleDescriptionBody[2];

  table.set_cursor_position(2);

  std::vector<InfolistWindowView::Entry> infolist_entries;
  size_t focused_index = 0;

  TestableCandidateWindowControllerImpl::ConvertLookupTableToInfolistEntry(
      table,
      &infolist_entries,
      &focused_index);

  // Infolist entries skips empty descriptions, so expected entry size is 1 and
  // expected focus index is 0.
  EXPECT_EQ(1UL, infolist_entries.size());
  EXPECT_EQ(0UL, focused_index);
}

TEST_F(CandidateWindowControllerImplTest,
       ConvertLookupTableToInfolistEntryTest_SparseNoSelectionCase) {
  ibus::IBusLookupTable table;
  table.set_page_size(10);

  for (size_t i = 0; i < kSampleCandidateSize; ++i) {
    ibus::IBusLookupTable::Entry entry;
    entry.value = kSampleCandidate[i];
    table.mutable_candidates()->push_back(entry);
  }

  std::vector<ibus::IBusLookupTable::Entry>* candidates =
      table.mutable_candidates();
  (*candidates)[2].description_title = kSampleDescriptionTitle[2];
  (*candidates)[2].description_body = kSampleDescriptionBody[2];

  table.set_cursor_position(0);

  std::vector<InfolistWindowView::Entry> infolist_entries;
  size_t focused_index = 0;

  TestableCandidateWindowControllerImpl::ConvertLookupTableToInfolistEntry(
      table,
      &infolist_entries,
      &focused_index);

  // Infolist entries skips empty descriptions, so expected entry size is 1 and
  // there is no focused index because no infolist entry candidate is selected.
  EXPECT_EQ(1UL, infolist_entries.size());
  EXPECT_EQ(static_cast<size_t>(-1), focused_index);
}

TEST_F(CandidateWindowControllerImplTest,
       ConvertLookupTableToInfolistEntryTest_NoInfolistCase) {
  ibus::IBusLookupTable table;
  table.set_page_size(10);

  for (size_t i = 0; i < kSampleCandidateSize; ++i) {
    ibus::IBusLookupTable::Entry entry;
    entry.value = kSampleCandidate[i];
    table.mutable_candidates()->push_back(entry);
  }
  table.set_cursor_position(1);

  std::vector<InfolistWindowView::Entry> infolist_entries;
  size_t focused_index = 0;

  TestableCandidateWindowControllerImpl::ConvertLookupTableToInfolistEntry(
      table,
      &infolist_entries,
      &focused_index);

  EXPECT_TRUE(infolist_entries.empty());
  EXPECT_EQ(static_cast<size_t>(-1), focused_index);
}

TEST_F(CandidateWindowControllerImplTest, ShouldUpdateInfolist_EmptyUpdate) {
  std::vector<InfolistWindowView::Entry> old_entry;
  std::vector<InfolistWindowView::Entry> new_entry;
  EXPECT_FALSE(TestableCandidateWindowControllerImpl::ShouldUpdateInfolist(
      old_entry,
      InfolistWindowView::InvalidFocusIndex(),
      new_entry,
      InfolistWindowView::InvalidFocusIndex()));
}

TEST_F(CandidateWindowControllerImplTest,
       ShouldUpdateInfolist_SameEntryUpdate) {
  std::vector<InfolistWindowView::Entry> old_entry;
  std::vector<InfolistWindowView::Entry> new_entry;
  InfolistWindowView::Entry entry;
  entry.title = kSampleDescriptionTitle[0];
  entry.body = kSampleDescriptionBody[0];

  old_entry.push_back(entry);
  new_entry.push_back(entry);

  EXPECT_FALSE(TestableCandidateWindowControllerImpl::ShouldUpdateInfolist(
      old_entry,
      InfolistWindowView::InvalidFocusIndex(),
      new_entry,
      InfolistWindowView::InvalidFocusIndex()));
}

TEST_F(CandidateWindowControllerImplTest,
       ShouldUpdateInfolist_FOcusIndexDifferentUpdate) {
  std::vector<InfolistWindowView::Entry> old_entry;
  std::vector<InfolistWindowView::Entry> new_entry;
  InfolistWindowView::Entry entry;
  entry.title = kSampleDescriptionTitle[0];
  entry.body = kSampleDescriptionBody[0];

  old_entry.push_back(entry);
  new_entry.push_back(entry);

  EXPECT_TRUE(TestableCandidateWindowControllerImpl::ShouldUpdateInfolist(
      old_entry,
      0UL,
      new_entry,
      InfolistWindowView::InvalidFocusIndex()));
}

TEST_F(CandidateWindowControllerImplTest,
       ShouldUpdateInfolist_DifferentEntryUpdate) {
  std::vector<InfolistWindowView::Entry> old_entry;
  std::vector<InfolistWindowView::Entry> new_entry;
  InfolistWindowView::Entry entry1;
  InfolistWindowView::Entry entry2;
  entry1.title = kSampleDescriptionTitle[0];
  entry1.body = kSampleDescriptionBody[0];
  old_entry.push_back(entry1);

  // Both title and body are different.
  entry2.title = kSampleDescriptionTitle[1];
  entry2.body = kSampleDescriptionBody[1];
  new_entry.push_back(entry2);
  EXPECT_TRUE(TestableCandidateWindowControllerImpl::ShouldUpdateInfolist(
      old_entry,
      InfolistWindowView::InvalidFocusIndex(),
      new_entry,
      InfolistWindowView::InvalidFocusIndex()));
  new_entry.clear();

  // Only title is different.
  entry2.title = kSampleDescriptionTitle[1];
  entry2.body = kSampleDescriptionBody[0];
  new_entry.push_back(entry2);
  EXPECT_TRUE(TestableCandidateWindowControllerImpl::ShouldUpdateInfolist(
      old_entry,
      InfolistWindowView::InvalidFocusIndex(),
      new_entry,
      InfolistWindowView::InvalidFocusIndex()));
  new_entry.clear();

  // Only body is different.
  entry2.title = kSampleDescriptionTitle[0];
  entry2.body = kSampleDescriptionBody[1];
  new_entry.push_back(entry2);
  EXPECT_TRUE(TestableCandidateWindowControllerImpl::ShouldUpdateInfolist(
      old_entry,
      InfolistWindowView::InvalidFocusIndex(),
      new_entry,
      InfolistWindowView::InvalidFocusIndex()));
  new_entry.clear();
}

}  // namespace input_method
}  // namespace chromeos

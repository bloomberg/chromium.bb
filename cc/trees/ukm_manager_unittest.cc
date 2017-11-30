// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/ukm_manager.h"

#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const char kTestUrl1[] = "chrome://test";
const char kTestUrl2[] = "chrome://test2";

const char kUserInteraction[] = "Compositor.UserInteraction";
const char kCheckerboardArea[] = "CheckerboardedContentArea";
const char kCheckerboardAreaRatio[] = "CheckerboardedContentAreaRatio";
const char kMissingTiles[] = "NumMissingTiles";

class UkmManagerTest : public testing::Test {
 public:
  UkmManagerTest() {
    auto recorder = std::make_unique<ukm::TestUkmRecorder>();
    test_ukm_recorder_ = recorder.get();
    manager_ = std::make_unique<UkmManager>(std::move(recorder));
    manager_->SetSourceURL(GURL(kTestUrl1));
  }

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder_;
  std::unique_ptr<UkmManager> manager_;
};

TEST_F(UkmManagerTest, Basic) {
  manager_->SetUserInteractionInProgress(true);
  manager_->AddCheckerboardStatsForFrame(5, 1, 10);
  manager_->AddCheckerboardStatsForFrame(15, 3, 30);
  manager_->SetUserInteractionInProgress(false);

  // We should see a single entry for the interaction above.
  const auto& entries = test_ukm_recorder_->GetEntriesByName(kUserInteraction);
  ukm::SourceId original_id = ukm::kInvalidSourceId;
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    original_id = entry->source_id;
    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl1));
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardArea, 10);
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissingTiles, 2);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardAreaRatio, 50);
  }
  test_ukm_recorder_->Purge();

  // Try pushing some stats while no user interaction is happening. No entries
  // should be pushed.
  manager_->AddCheckerboardStatsForFrame(6, 1, 10);
  manager_->AddCheckerboardStatsForFrame(99, 3, 100);
  EXPECT_EQ(0u, test_ukm_recorder_->entries_count());
  manager_->SetUserInteractionInProgress(true);
  EXPECT_EQ(0u, test_ukm_recorder_->entries_count());

  // Record a few entries and change the source before the interaction ends. The
  // stats collected up till this point should be recorded before the source is
  // swapped.
  manager_->AddCheckerboardStatsForFrame(10, 1, 100);
  manager_->AddCheckerboardStatsForFrame(30, 5, 100);

  manager_->SetSourceURL(GURL(kTestUrl2));

  const auto& entries2 = test_ukm_recorder_->GetEntriesByName(kUserInteraction);
  EXPECT_EQ(1u, entries2.size());
  for (const auto* entry : entries2) {
    EXPECT_EQ(original_id, entry->source_id);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardArea, 20);
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissingTiles, 3);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardAreaRatio, 20);
  }
}

}  // namespace
}  // namespace cc

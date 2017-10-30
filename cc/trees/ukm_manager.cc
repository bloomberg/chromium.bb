// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/ukm_manager.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {

UkmManager::UkmManager(std::unique_ptr<ukm::UkmRecorder> recorder)
    : recorder_(std::move(recorder)) {
  DCHECK(recorder_);
}

UkmManager::~UkmManager() = default;

void UkmManager::SetSourceURL(const GURL& url) {
  // If we accumulating any metrics, record them before reseting the source.
  RecordCheckerboardUkm();

  source_id_ = recorder_->GetNewSourceID();
  recorder_->UpdateSourceURL(source_id_, url);
}

void UkmManager::SetUserInteractionInProgress(bool in_progress) {
  if (user_interaction_in_progress_ == in_progress)
    return;

  user_interaction_in_progress_ = in_progress;
  if (!user_interaction_in_progress_)
    RecordCheckerboardUkm();
}

void UkmManager::AddCheckerboardStatsForFrame(int64_t checkerboard_area,
                                              int64_t num_missing_tiles,
                                              int64_t total_visible_area) {
  DCHECK_GE(total_visible_area, checkerboard_area);
  if (source_id_ == ukm::kInvalidSourceId || !user_interaction_in_progress_)
    return;

  checkerboarded_content_area_ += checkerboard_area;
  num_missing_tiles_ += num_missing_tiles;
  total_visible_area_ += total_visible_area;
  num_of_frames_++;
}

void UkmManager::RecordCheckerboardUkm() {
  // Only make a recording if there was any visible area from PictureLayers,
  // which can be checkerboarded.
  if (num_of_frames_ > 0 && total_visible_area_ > 0) {
    DCHECK_NE(source_id_, ukm::kInvalidSourceId);
    ukm::builders::Compositor_UserInteraction(source_id_)
        .SetCheckerboardedContentArea(checkerboarded_content_area_ /
                                      num_of_frames_)
        .SetNumMissingTiles(num_missing_tiles_ / num_of_frames_)
        .SetCheckerboardedContentAreaRatio(
            (checkerboarded_content_area_ * 100) / total_visible_area_)
        .Record(recorder_.get());
  }

  checkerboarded_content_area_ = 0;
  num_missing_tiles_ = 0;
  num_of_frames_ = 0;
  total_visible_area_ = 0;
}

}  // namespace cc

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/learning_helper.h"

#include "media/learning/common/learning_task.h"

namespace media {

using learning::FeatureValue;
using learning::LabelledExample;
using learning::LearningTask;
using learning::TargetValue;

const char* kDroppedFrameRatioTaskName = "DroppedFrameRatioTask";

LearningHelper::LearningHelper() {
  // Register a few learning tasks.
  //
  // We only do this here since we own the session.  Normally, whatever creates
  // the session would register all the learning tasks.
  LearningTask dropped_frame_task(
      kDroppedFrameRatioTaskName, LearningTask::Model::kExtraTrees,
      {
          {"codec_profile",
           ::media::learning::LearningTask::Ordering::kUnordered},
          {"width", ::media::learning::LearningTask::Ordering::kNumeric},
          {"height", ::media::learning::LearningTask::Ordering::kNumeric},
          {"frame_rate", ::media::learning::LearningTask::Ordering::kNumeric},
      },
      LearningTask::ValueDescription(
          {"dropped_ratio", LearningTask::Ordering::kNumeric}));
  // Enable hacky reporting of accuracy.
  dropped_frame_task.uma_hacky_confusion_matrix =
      "Media.Learning.MediaCapabilities.DroppedFrameRatioTask.BaseTree";
  learning_session_.RegisterTask(dropped_frame_task);
}

LearningHelper::~LearningHelper() = default;

void LearningHelper::AppendStats(
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    const VideoDecodeStatsDB::DecodeStatsEntry& new_stats) {
  // If no frames were recorded, then do nothing.
  if (new_stats.frames_decoded == 0)
    return;

  // Sanity.
  if (new_stats.frames_dropped > new_stats.frames_decoded)
    return;

  // Add a training example for |new_stats|.
  LabelledExample example;

  // Extract features from |video_key|.
  example.features.push_back(FeatureValue(video_key.codec_profile));
  example.features.push_back(FeatureValue(video_key.size.width()));
  example.features.push_back(FeatureValue(video_key.size.height()));
  example.features.push_back(FeatureValue(video_key.frame_rate));
  // TODO(liberato): Other features?

  // Record the ratio of dropped frames to non-dropped frames.  Weight this
  // example by the total number of frames, since we want to predict the
  // aggregate dropped frames ratio.  That lets us compare with the current
  // implementation directly.
  //
  // It's also not clear that we want to do this; we might want to weight each
  // playback equally and predict the dropped frame ratio.  For example, if
  // there is a dependence on video length, then it's unclear that weighting
  // the examples is the right thing to do.
  example.target_value = TargetValue(
      static_cast<double>(new_stats.frames_dropped) / new_stats.frames_decoded);
  example.weight = new_stats.frames_decoded;
  learning_session_.AddExample(kDroppedFrameRatioTaskName, example);
}

}  // namespace media

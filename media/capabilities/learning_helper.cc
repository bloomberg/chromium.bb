// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/learning_helper.h"

#include "base/task/post_task.h"
#include "media/learning/common/feature_library.h"
#include "media/learning/common/learning_task.h"

namespace media {

using learning::FeatureLibrary;
using learning::FeatureProviderFactoryCB;
using learning::FeatureValue;
using learning::LabelledExample;
using learning::LearningSessionImpl;
using learning::LearningTask;
using learning::SequenceBoundFeatureProvider;
using learning::TargetValue;

// Dropped frame ratio, default features, regression tree.
const char* const kDroppedFrameRatioBaseTreeTaskName =
    "DroppedFrameRatioBaseTreeTask";
// Dropped frame ratio, default+FeatureLibrary features, regression tree.
const char* const kDroppedFrameRatioEnhancedTreeTaskName =
    "DroppedFrameRatioEnhancedTreeTask";
// Dropped frame ratio, default features, lookup table.
const char* const kDroppedFrameRatioBaseTableTaskName =
    "DroppedFrameRatioBaseTableTask";

LearningHelper::LearningHelper(FeatureProviderFactoryCB feature_factory) {
  // Create the LearningSession on a background task runner.  In the future,
  // it's likely that the session will live on the main thread, and handle
  // delegation of LearningTaskControllers to other threads.  However, for now,
  // do it here.
  learning_session_ = std::make_unique<LearningSessionImpl>(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  // Register a few learning tasks.
  //
  // We only do this here since we own the session.  Normally, whatever creates
  // the session would register all the learning tasks.
  LearningTask dropped_frame_task(
      kDroppedFrameRatioBaseTableTaskName, LearningTask::Model::kLookupTable,
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
  learning_session_->RegisterTask(dropped_frame_task,
                                  SequenceBoundFeatureProvider());

  // Modify the task to use ExtraTrees.
  dropped_frame_task.name = kDroppedFrameRatioBaseTreeTaskName;
  dropped_frame_task.model = LearningTask::Model::kExtraTrees;
  dropped_frame_task.uma_hacky_confusion_matrix =
      "Media.Learning.MediaCapabilities.DroppedFrameRatioTask.BaseTable";
  learning_session_->RegisterTask(dropped_frame_task,
                                  SequenceBoundFeatureProvider());

  // Add common features, if we have a factory.
  if (feature_factory) {
    dropped_frame_task.name = kDroppedFrameRatioEnhancedTreeTaskName;
    dropped_frame_task.feature_descriptions.push_back(
        FeatureLibrary::NetworkType());
    dropped_frame_task.feature_descriptions.push_back(
        FeatureLibrary::BatteryPower());
    dropped_frame_task.uma_hacky_confusion_matrix =
        "Media.Learning.MediaCapabilities.DroppedFrameRatioTask.EnhancedTree";
    learning_session_->RegisterTask(dropped_frame_task,
                                    feature_factory.Run(dropped_frame_task));
  }
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

  // Add this example to both tasks.
  learning_session_->AddExample(kDroppedFrameRatioBaseTreeTaskName, example);
  learning_session_->AddExample(kDroppedFrameRatioBaseTableTaskName, example);
  // Might fail, but that's okay.
  learning_session_->AddExample(kDroppedFrameRatioEnhancedTreeTaskName,
                                example);
}

}  // namespace media

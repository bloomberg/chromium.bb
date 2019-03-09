// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_LEARNING_HELPER_H_
#define MEDIA_CAPABILITIES_LEARNING_HELPER_H_

#include "base/macros.h"
#include "base/threading/sequence_bound.h"
#include "media/base/media_export.h"
#include "media/capabilities/video_decode_stats_db.h"
#include "media/learning/impl/feature_provider.h"
#include "media/learning/impl/learning_session_impl.h"

namespace media {

// Helper class to allow MediaCapabilities to log training examples to a
// media::learning LearningTask.
class MEDIA_EXPORT LearningHelper {
 public:
  // |feature_factory| lets us register FeatureProviders with those
  // LearningTasks that include standard features.
  LearningHelper(learning::FeatureProviderFactoryCB feature_factory);
  ~LearningHelper();

  void AppendStats(const VideoDecodeStatsDB::VideoDescKey& video_key,
                   const VideoDecodeStatsDB::DecodeStatsEntry& new_stats);

 private:
  // Learning session for our profile.  This isn't the way LearningSession is
  // intended to be used -- one should expect to get a LearningTaskController
  // for a particular task.  The LearningSession would be owned elsewhere (e.g.,
  // the BrowserContext).  We do it this way here since LearningHelper is an
  // hacky way to see if all this works.
  std::unique_ptr<learning::LearningSessionImpl> learning_session_;
};

}  // namespace media

#endif  // MEDIA_CAPABILITIES_LEARNING_HELPER_H_

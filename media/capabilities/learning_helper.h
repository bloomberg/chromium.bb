// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_LEARNING_HELPER_H_
#define MEDIA_CAPABILITIES_LEARNING_HELPER_H_

#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/capabilities/video_decode_stats_db.h"
#include "media/learning/impl/learning_session_impl.h"

namespace media {

// Helper class to allow MediaCapabilities to log training examples to a
// media::learning LearningTask.
class MEDIA_EXPORT LearningHelper {
 public:
  LearningHelper();
  ~LearningHelper();

  void AppendStats(const VideoDecodeStatsDB::VideoDescKey& video_key,
                   const VideoDecodeStatsDB::DecodeStatsEntry& new_stats);

 private:
  // Learning session for our profile.  Normally, we'd not have one of these
  // directly, but would instead get one that's connected to a browser profile.
  // For now, however, we just instantiate one and assume that we'll be
  // destroyed when the profile changes / history is cleared.
  learning::LearningSessionImpl learning_session_;
};

}  // namespace media

#endif  // MEDIA_CAPABILITIES_LEARNING_HELPER_H_

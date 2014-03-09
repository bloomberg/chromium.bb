// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_STANDALONE_CAST_ENVIRONMENT_H_
#define MEDIA_CAST_TEST_UTILITY_STANDALONE_CAST_ENVIRONMENT_H_

#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/cast/cast_environment.h"

namespace media {
namespace cast {

// A complete CastEnvironment where all task runners are spurned from
// internally-owned threads.  Uses base::DefaultTickClock as a clock.
class StandaloneCastEnvironment : public CastEnvironment,
                                  public base::ThreadChecker {
 public:
  explicit StandaloneCastEnvironment(const CastLoggingConfig& logging_config);

  // Stops all threads backing the task runners, blocking the caller until
  // complete.
  void Shutdown();

 private:
  virtual ~StandaloneCastEnvironment();

  base::Thread main_thread_;
  base::Thread audio_encode_thread_;
  base::Thread audio_decode_thread_;
  base::Thread video_encode_thread_;
  base::Thread video_decode_thread_;
  base::Thread transport_thread_;

  DISALLOW_COPY_AND_ASSIGN(StandaloneCastEnvironment);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_STANDALONE_CAST_ENVIRONMENT_H_

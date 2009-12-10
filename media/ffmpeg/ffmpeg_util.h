// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_FFMPEG_FFMPEG_UTIL_H_
#define MEDIA_FFMPEG_FFMPEG_UTIL_H_

#include "base/time.h"

struct AVRational;

namespace media {

base::TimeDelta ConvertTimestamp(const AVRational& time_base, int64 timestamp);

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_UTIL_H_

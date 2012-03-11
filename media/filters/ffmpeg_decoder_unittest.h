// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_DECODER_UNITTEST_H_
#define MEDIA_FILTERS_FFMPEG_DECODER_UNITTEST_H_

namespace media {
// Used to inject our message loop into the FFmpeg[Audio|Video|Decoder.
template<class T> T Identity(T t) { return t; }
}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DECODER_UNITTEST_H_

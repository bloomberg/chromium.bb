// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_sink.h"

namespace media_router {

MediaSink::MediaSink(const MediaSinkId& sink_id,
                     const std::string& name)
    : sink_id_(sink_id), name_(name) {
}

MediaSink::~MediaSink() {
}

bool MediaSink::Equals(const MediaSink& other) const {
  return sink_id_ == other.sink_id_;
}

}  // namespace media_router

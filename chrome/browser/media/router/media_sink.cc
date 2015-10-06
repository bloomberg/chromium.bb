// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_sink.h"

namespace media_router {

MediaSink::MediaSink(const MediaSink::Id& sink_id,
                     const std::string& name,
                     const MediaSink::IconType icon_type)
    : sink_id_(sink_id), name_(name), icon_type_(icon_type) {}

MediaSink::~MediaSink() {
}

bool MediaSink::Equals(const MediaSink& other) const {
  return sink_id_ == other.sink_id_;
}

bool MediaSink::Empty() const {
  return sink_id_.empty();
}

}  // namespace media_router

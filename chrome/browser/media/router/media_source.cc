// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source.h"

#include <string>

namespace media_router {

MediaSource::MediaSource(const MediaSource::Id& source_id) : id_(source_id) {
}

MediaSource::MediaSource() {
}

MediaSource::~MediaSource() {}

MediaSource::Id MediaSource::id() const {
  return id_;
}

bool MediaSource::Equals(const MediaSource& other) const {
  return id_ == other.id();
}

bool MediaSource::Empty() const {
  return id_.empty();
}

std::string MediaSource::ToString() const {
  return "MediaSource[" + id_ + "]";
}

}  // namespace media_router

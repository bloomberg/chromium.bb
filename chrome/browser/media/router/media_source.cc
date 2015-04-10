// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source.h"

#include <string>

namespace media_router {

MediaSource::MediaSource(const std::string& source_id)
    : id_(source_id) {
}

MediaSource::~MediaSource() {}

std::string MediaSource::id() const {
  return id_;
}

}  // namespace media_router

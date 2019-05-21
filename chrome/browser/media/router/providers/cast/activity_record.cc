// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/activity_record.h"

namespace media_router {

ActivityRecord::ActivityRecord(const MediaRoute& route,
                               const std::string& app_id)
    : route_(route), app_id_(app_id) {}

ActivityRecord::~ActivityRecord() = default;

}  // namespace media_router

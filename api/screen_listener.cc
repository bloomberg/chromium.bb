// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/screen_listener.h"

namespace openscreen {

ScreenListenerErrorInfo::ScreenListenerErrorInfo() : error(ScreenListenerError::kNone) {}

ScreenListenerErrorInfo::ScreenListenerErrorInfo(ScreenListenerError error, const std::string& message) :
  error(error), message(message) {}

ScreenListenerErrorInfo::~ScreenListenerErrorInfo() = default;

ScreenListenerMetrics::ScreenListenerMetrics() = default;

ScreenListenerMetrics::~ScreenListenerMetrics() = default;

ScreenInfo::ScreenInfo(const std::string& screen_id) : screen_id(screen_id) {}

ScreenInfo::~ScreenInfo() = default;

ScreenListener::~ScreenListener() = default;

ScreenListener::ScreenListener()  = default;

}  // namespace openscreen

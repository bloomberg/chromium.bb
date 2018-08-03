// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_listener.h"

namespace openscreen {

ScreenListenerErrorInfo::ScreenListenerErrorInfo() = default;
ScreenListenerErrorInfo::ScreenListenerErrorInfo(ScreenListenerError error,
                                                 const std::string& message)
    : error(error), message(message) {}
ScreenListenerErrorInfo::ScreenListenerErrorInfo(
    const ScreenListenerErrorInfo& other) = default;
ScreenListenerErrorInfo::~ScreenListenerErrorInfo() = default;

ScreenListenerErrorInfo& ScreenListenerErrorInfo::operator=(
    const ScreenListenerErrorInfo& other) = default;

ScreenListenerMetrics::ScreenListenerMetrics() = default;
ScreenListenerMetrics::~ScreenListenerMetrics() = default;

ScreenListener::ScreenListener(ScreenListenerObserver* observer)
    : observer_(observer) {}
ScreenListener::~ScreenListener() = default;

}  // namespace openscreen

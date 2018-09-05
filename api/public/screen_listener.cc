// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_listener.h"

namespace openscreen {

ScreenListenerError::ScreenListenerError() = default;
ScreenListenerError::ScreenListenerError(Code error, const std::string& message)
    : error(error), message(message) {}
ScreenListenerError::ScreenListenerError(const ScreenListenerError& other) =
    default;
ScreenListenerError::~ScreenListenerError() = default;

ScreenListenerError& ScreenListenerError::operator=(
    const ScreenListenerError& other) = default;

ScreenListener::Metrics::Metrics() = default;
ScreenListener::Metrics::~Metrics() = default;

ScreenListener::ScreenListener(Observer* observer) : observer_(observer) {}
ScreenListener::~ScreenListener() = default;

}  // namespace openscreen

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_publisher.h"

namespace openscreen {

ScreenPublisherError::ScreenPublisherError() = default;
ScreenPublisherError::ScreenPublisherError(Code error,
                                           const std::string& message)
    : error(error), message(message) {}
ScreenPublisherError::ScreenPublisherError(const ScreenPublisherError& other) =
    default;
ScreenPublisherError::~ScreenPublisherError() = default;

ScreenPublisherError& ScreenPublisherError::operator=(
    const ScreenPublisherError& other) = default;

ScreenPublisher::Metrics::Metrics() = default;
ScreenPublisher::Metrics::~Metrics() = default;

ScreenPublisher::Config::Config() = default;
ScreenPublisher::Config::~Config() = default;

ScreenPublisher::ScreenPublisher(Observer* observer) : observer_(observer) {}
ScreenPublisher::~ScreenPublisher() = default;

}  // namespace openscreen

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/mdns_screen_publisher_factory.h"

#include "api/impl/internal_services.h"

namespace openscreen {

// static
std::unique_ptr<ScreenPublisher> MdnsScreenPublisherFactory::Create(
    const ScreenPublisher::Config& config,
    ScreenPublisher::Observer* observer) {
  return InternalServices::CreatePublisher(config, observer);
}

}  // namespace openscreen

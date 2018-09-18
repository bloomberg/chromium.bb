// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_MDNS_SCREEN_PUBLISHER_FACTORY_H_
#define API_IMPL_MDNS_SCREEN_PUBLISHER_FACTORY_H_

#include <memory>

#include "api/public/screen_publisher.h"

namespace openscreen {

class MdnsScreenPublisherFactory {
 public:
  static std::unique_ptr<ScreenPublisher> Create(
      const ScreenPublisher::Config& config,
      ScreenPublisher::Observer* observer);
};

}  // namespace openscreen

#endif  // API_IMPL_MDNS_SCREEN_PUBLISHER_FACTORY_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_MDNS_SCREEN_LISTENER_FACTORY_H_
#define API_IMPL_MDNS_SCREEN_LISTENER_FACTORY_H_

#include <memory>

#include "api/public/screen_listener.h"

namespace openscreen {

struct MdnsScreenListenerConfig {
  // TODO(mfoltz): Populate with actual parameters as implementation progresses.
  bool dummy_value = true;
};

class MdnsScreenListenerFactory {
 public:
  static std::unique_ptr<ScreenListener> Create(
      const MdnsScreenListenerConfig& config,
      ScreenListener::Observer* observer);
};

}  // namespace openscreen

#endif  // API_IMPL_MDNS_SCREEN_LISTENER_FACTORY_H_

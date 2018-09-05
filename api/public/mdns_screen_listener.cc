// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/mdns_screen_listener.h"

namespace openscreen {

MdnsScreenListener::MdnsScreenListener(Observer* observer,
                                       MdnsScreenListenerConfig config)
    : ScreenListener(observer), config_(config) {}

MdnsScreenListener::~MdnsScreenListener() = default;

}  // namespace openscreen

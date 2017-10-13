// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_discovery.h"

#include <utility>

namespace device {

U2fDiscovery::U2fDiscovery() = default;

U2fDiscovery::~U2fDiscovery() = default;

void U2fDiscovery::SetDelegate(base::WeakPtr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

}  // namespace device

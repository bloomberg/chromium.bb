// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_DATA_CHANNEL_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_DATA_CHANNEL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace devtools_bridge {

/**
 * WebRTC DataChannel adapter for DevTools bridge.
 */
class AbstractDataChannel {
 public:
  AbstractDataChannel() {}
  virtual ~AbstractDataChannel() {}

  // TODO(serya): Implement

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractDataChannel);
};

}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_DATA_CHANNEL_H_

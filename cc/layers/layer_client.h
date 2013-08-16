// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_CLIENT_H_
#define CC_LAYERS_LAYER_CLIENT_H_

#include <string>

#include "cc/base/cc_export.h"

namespace cc {

class CC_EXPORT LayerClient {
 public:
  virtual std::string DebugName() = 0;

 protected:
  virtual ~LayerClient() {}
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_CLIENT_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_LAYER_FACTORY_H_
#define CC_BLIMP_LAYER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace cc {
class Layer;

// Used to allow tests to inject the Layer created by the
// CompositorStateDeserializer on the client.
class LayerFactory {
 public:
  virtual ~LayerFactory() {}

  virtual scoped_refptr<Layer> CreateLayer(int engine_layer_id) = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_LAYER_FACTORY_H_

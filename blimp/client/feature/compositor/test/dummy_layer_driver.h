// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_TEST_DUMMY_LAYER_DRIVER_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_TEST_DUMMY_LAYER_DRIVER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace cc {
class Layer;
}

namespace blimp {
namespace client {

// A test class that drives changes to a layer every 16ms.  This can be used to
// make sure the compositor is rendering layers properly.  In this case, colors
// are constantly changed over time.
class DummyLayerDriver {
 public:
  DummyLayerDriver();
  virtual ~DummyLayerDriver();

  // Adds a cc::SolidColorLayer to |layer| and animates it's color until the
  // layer is detached from it's cc::LayerTreeHost.
  void SetParentLayer(scoped_refptr<cc::Layer> layer);

 private:
  void StepAnimation();

  scoped_refptr<cc::Layer> layer_;
  bool animation_running_;

  base::WeakPtrFactory<DummyLayerDriver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DummyLayerDriver);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_TEST_DUMMY_LAYER_DRIVER_H_

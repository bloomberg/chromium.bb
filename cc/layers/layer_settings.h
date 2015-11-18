// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_SETTINGS_H_
#define CC_LAYERS_LAYER_SETTINGS_H_

#include "cc/base/cc_export.h"

namespace cc {

class CC_EXPORT LayerSettings {
 public:
  LayerSettings();
  ~LayerSettings();

  bool use_compositor_animation_timelines;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_SETTINGS_H_

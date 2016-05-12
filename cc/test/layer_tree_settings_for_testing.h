// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_SETTINGS_FOR_TESTING_H_
#define CC_TEST_LAYER_TREE_SETTINGS_FOR_TESTING_H_

#include "cc/trees/layer_tree_settings.h"

namespace cc {

class LayerTreeSettingsForTesting : public LayerTreeSettings {
 public:
  LayerTreeSettingsForTesting();
  ~LayerTreeSettingsForTesting() override;
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_SETTINGS_FOR_TESTING_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_LAYER_TREE_MUTATOR_H_
#define CC_ANIMATION_LAYER_TREE_MUTATOR_H_

#include "base/callback_forward.h"
#include "cc/base/cc_export.h"

namespace cc {

class CC_EXPORT LayerTreeMutator {
 public:
  virtual ~LayerTreeMutator() {}

  // Returns a callback which is responsible for applying layer tree mutations
  // to DOM elements.
  virtual base::Closure TakeMutations() = 0;
};

}  // namespace cc

#endif  // CC_ANIMATION_LAYER_TREE_MUTATOR_H_

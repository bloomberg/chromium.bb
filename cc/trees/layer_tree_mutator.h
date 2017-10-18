// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_MUTATOR_H_
#define CC_TREES_LAYER_TREE_MUTATOR_H_

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT LayerTreeMutator {
 public:
  virtual ~LayerTreeMutator() {}

  virtual void Mutate(base::TimeTicks now) = 0;
  // TODO(majidvp): Remove when timeline inputs are known.
  virtual bool HasAnimators() = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_MUTATOR_H_

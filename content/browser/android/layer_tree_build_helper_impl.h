// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_LAYER_TREE_BUILD_HELPER_IMPL_H_
#define CONTENT_BROWSER_ANDROID_LAYER_TREE_BUILD_HELPER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/android/layer_tree_build_helper.h"

namespace cc {
class Layer;
}

namespace content {

// A Helper class to build a layer tree to be composited
// given a content root layer.
class LayerTreeBuildHelperImpl : public LayerTreeBuildHelper {
 public:
  LayerTreeBuildHelperImpl();

  virtual ~LayerTreeBuildHelperImpl();

  virtual scoped_refptr<cc::Layer> GetLayerTree(
      scoped_refptr<cc::Layer> content_root_layer) OVERRIDE;

 private:
  scoped_refptr<cc::Layer> root_layer_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeBuildHelperImpl);
};

}

#endif  // CONTENT_BROWSER_ANDROID_LAYER_TREE_BUILD_HELPER_IMPL_H_

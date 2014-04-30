// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/layer_tree_build_helper_impl.h"

#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"

namespace content {

LayerTreeBuildHelperImpl::LayerTreeBuildHelperImpl() {
  root_layer_ = cc::SolidColorLayer::Create();
  root_layer_->SetIsDrawable(true);
  root_layer_->SetBackgroundColor(SK_ColorWHITE);
}

LayerTreeBuildHelperImpl::~LayerTreeBuildHelperImpl() {
}

scoped_refptr<cc::Layer> LayerTreeBuildHelperImpl::GetLayerTree(
    scoped_refptr<cc::Layer> content_root_layer) {
  if (content_root_layer)
    root_layer_ = content_root_layer;
  else
    root_layer_ = cc::Layer::Create();

  return root_layer_;
}
}  // namespace content

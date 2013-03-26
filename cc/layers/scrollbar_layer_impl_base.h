// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_
#define CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_

#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"

namespace cc {

class CC_EXPORT ScrollbarLayerImplBase : public LayerImpl {
 public:
  virtual ~ScrollbarLayerImplBase() {}

  virtual float CurrentPos() const = 0;
  virtual int TotalSize() const = 0;
  virtual int Maximum() const = 0;
  virtual WebKit::WebScrollbar::Orientation Orientation() const = 0;

 protected:
  ScrollbarLayerImplBase(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}
};

}  // namespace cc

#endif  // CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_

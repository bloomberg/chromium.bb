// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_NINE_PATCH_LAYER_IMPL_H_
#define CC_NINE_PATCH_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "cc/resource_provider.h"
#include "ui/gfx/size.h"
#include "ui/gfx/rect.h"

namespace base {
class DictionaryValue;
}

namespace cc {

class CC_EXPORT NinePatchLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<NinePatchLayerImpl> Create(LayerTreeImpl* tree_impl,
                                               int id) {
    return make_scoped_ptr(new NinePatchLayerImpl(tree_impl, id));
  }
  virtual ~NinePatchLayerImpl();

  void SetResourceId(unsigned id) { resource_id_ = id; }
  void SetLayout(gfx::Size image_bounds, gfx::Rect aperture);

  virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void pushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual void willDraw(ResourceProvider* resource_provider) OVERRIDE;
  virtual void appendQuads(QuadSink& quad_sink,
                           AppendQuadsData& append_quads_data) OVERRIDE;
  virtual void didDraw(ResourceProvider* FIXMENAME) OVERRIDE;
  virtual ResourceProvider::ResourceId contentsResourceId() const OVERRIDE;
  virtual void dumpLayerProperties(std::string* str, int indent) const OVERRIDE;
  virtual void didLoseOutputSurface() OVERRIDE;

  virtual base::DictionaryValue* layerTreeAsJson() const OVERRIDE;

 protected:
  NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  virtual const char* layerTypeAsString() const OVERRIDE;

  // The size of the NinePatch bitmap in pixels.
  gfx::Size image_bounds_;

  // The transparent center region that shows the parent layer's contents in
  // image space.
  gfx::Rect image_aperture_;

  ResourceProvider::ResourceId resource_id_;
};

}  // namespace cc

#endif  // CC_NINE_PATCH_LAYER_IMPL_H_

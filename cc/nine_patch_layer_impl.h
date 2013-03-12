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

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual void WillDraw(ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  virtual void DidDraw(ResourceProvider* FIXMENAME) OVERRIDE;
  virtual ResourceProvider::ResourceId ContentsResourceId() const OVERRIDE;
  virtual void DumpLayerProperties(std::string* str, int indent) const OVERRIDE;
  virtual void DidLoseOutputSurface() OVERRIDE;

  virtual base::DictionaryValue* LayerTreeAsJson() const OVERRIDE;

 protected:
  NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  virtual const char* LayerTypeAsString() const OVERRIDE;

  // The size of the NinePatch bitmap in pixels.
  gfx::Size image_bounds_;

  // The transparent center region that shows the parent layer's contents in
  // image space.
  gfx::Rect image_aperture_;

  ResourceProvider::ResourceId resource_id_;
};

}  // namespace cc

#endif  // CC_NINE_PATCH_LAYER_IMPL_H_

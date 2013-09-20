// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer.h"

#include "cc/layers/nine_patch_layer_impl.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {


namespace {

class ScopedUIResourceHolder : public NinePatchLayer::UIResourceHolder {
 public:
  static scoped_ptr<ScopedUIResourceHolder> Create(LayerTreeHost* host,
                                            const SkBitmap& skbitmap) {
    return make_scoped_ptr(new ScopedUIResourceHolder(host, skbitmap));
  }
  virtual UIResourceId id() OVERRIDE { return resource_->id(); }

 private:
  ScopedUIResourceHolder(LayerTreeHost* host, const SkBitmap& skbitmap) {
    resource_ = ScopedUIResource::Create(host, UIResourceBitmap(skbitmap));
  }

  scoped_ptr<ScopedUIResource> resource_;
};

class SharedUIResourceHolder : public NinePatchLayer::UIResourceHolder {
 public:
  static scoped_ptr<SharedUIResourceHolder> Create(UIResourceId id) {
    return make_scoped_ptr(new SharedUIResourceHolder(id));
  }

  virtual UIResourceId id() OVERRIDE { return id_; }

 private:
  explicit SharedUIResourceHolder(UIResourceId id) : id_(id) {}

  UIResourceId id_;
};

}  // anonymous namespace


NinePatchLayer::UIResourceHolder::~UIResourceHolder() {}

scoped_refptr<NinePatchLayer> NinePatchLayer::Create() {
  return make_scoped_refptr(new NinePatchLayer());
}

NinePatchLayer::NinePatchLayer() : fill_center_(false) {}

NinePatchLayer::~NinePatchLayer() {}

scoped_ptr<LayerImpl> NinePatchLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return NinePatchLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void NinePatchLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (host == layer_tree_host())
    return;

  Layer::SetLayerTreeHost(host);

  // Recreate the resource hold against the new LTH.
  RecreateUIResourceHolder();
}

void NinePatchLayer::RecreateUIResourceHolder() {
  ui_resource_holder_.reset();
  if (!layer_tree_host() || bitmap_.empty())
    return;

  ui_resource_holder_ =
    ScopedUIResourceHolder::Create(layer_tree_host(), bitmap_);
}

void NinePatchLayer::SetBorder(gfx::Rect border) {
  if (border == border_)
    return;
  border_ = border;
  SetNeedsCommit();
}

void NinePatchLayer::SetBitmap(const SkBitmap& skbitmap, gfx::Rect aperture) {
  image_aperture_ = aperture;
  bitmap_ = skbitmap;

  // TODO(ccameron): Remove this. This provides the default border that was
  // provided before borders were required to be explicitly provided. Once Blink
  // fixes its callers to call SetBorder, this can be removed.
  SetBorder(gfx::Rect(aperture.x(),
                      aperture.y(),
                      skbitmap.width() - aperture.width(),
                      skbitmap.height() - aperture.height()));
  RecreateUIResourceHolder();
  SetNeedsCommit();
}

void NinePatchLayer::SetUIResourceId(UIResourceId resource_id,
                                     gfx::Rect aperture) {
  if (ui_resource_holder_ && ui_resource_holder_->id() == resource_id &&
      image_aperture_ == aperture)
    return;

  image_aperture_ = aperture;
  if (resource_id) {
    ui_resource_holder_ = SharedUIResourceHolder::Create(resource_id);
  } else {
    ui_resource_holder_.reset();
  }

  SetNeedsCommit();
}

void NinePatchLayer::SetFillCenter(bool fill_center) {
  if (fill_center_ == fill_center)
    return;

  fill_center_ = fill_center;
  SetNeedsCommit();
}

bool NinePatchLayer::DrawsContent() const {
  return ui_resource_holder_ && ui_resource_holder_->id() &&
         Layer::DrawsContent();
}

void NinePatchLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  NinePatchLayerImpl* layer_impl = static_cast<NinePatchLayerImpl*>(layer);

  if (!ui_resource_holder_) {
    layer_impl->SetUIResourceId(0);
  } else {
    DCHECK(layer_tree_host());

    gfx::Size image_size =
        layer_tree_host()->GetUIResourceSize(ui_resource_holder_->id());
    layer_impl->SetUIResourceId(ui_resource_holder_->id());
    layer_impl->SetLayout(image_size, image_aperture_, border_, fill_center_);
  }
}

}  // namespace cc

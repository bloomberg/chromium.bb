// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/wm/forwarding_layer_delegate.h"
#include "ash/common/wm/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"

namespace ash {
namespace wm {
namespace {

void EnsureAllChildrenAreVisible(ui::Layer* layer) {
  std::list<ui::Layer*> layers;
  layers.push_back(layer);
  while (!layers.empty()) {
    for (auto child : layers.front()->children())
      layers.push_back(child);
    layers.front()->SetVisible(true);
    layers.pop_front();
  }
}

}  // namespace

WindowMirrorView::WindowMirrorView(WmWindowAura* window) : target_(window) {
  DCHECK(window);
}
WindowMirrorView::~WindowMirrorView() {}

void WindowMirrorView::Init() {
  SetPaintToLayer(true);

  layer_owner_ = ::wm::RecreateLayers(target_->aura_window(), this);

  GetMirrorLayer()->parent()->Remove(GetMirrorLayer());
  layer()->Add(GetMirrorLayer());

  // Some extra work is needed when the target window is minimized.
  if (target_->GetWindowState()->IsMinimized()) {
    GetMirrorLayer()->SetVisible(true);
    GetMirrorLayer()->SetOpacity(1);
    EnsureAllChildrenAreVisible(GetMirrorLayer());
  }
}

gfx::Size WindowMirrorView::GetPreferredSize() const {
  const int kMaxWidth = 512;
  const int kMaxHeight = 256;

  gfx::Size target_size = target_->GetBounds().size();
  if (target_size.width() <= kMaxWidth && target_size.height() <= kMaxHeight) {
    return target_size;
  }

  float scale = std::min(kMaxWidth / static_cast<float>(target_size.width()),
                         kMaxHeight / static_cast<float>(target_size.height()));
  return gfx::ScaleToCeiledSize(target_size, scale, scale);
}

void WindowMirrorView::Layout() {
  // Position at 0, 0.
  GetMirrorLayer()->SetBounds(gfx::Rect(GetMirrorLayer()->bounds().size()));

  // Scale down if necessary.
  gfx::Transform mirror_transform;
  if (size() != target_->GetBounds().size()) {
    const float scale =
        width() / static_cast<float>(target_->GetBounds().width());
    mirror_transform.Scale(scale, scale);
  }
  GetMirrorLayer()->SetTransform(mirror_transform);
}

ui::LayerDelegate* WindowMirrorView::CreateDelegate(
    ui::LayerDelegate* delegate) {
  if (!delegate)
    return nullptr;
  delegates_.push_back(
      base::WrapUnique(new ForwardingLayerDelegate(target_, delegate)));

  return delegates_.back().get();
}

ui::Layer* WindowMirrorView::GetMirrorLayer() {
  return layer_owner_->root();
}

}  // namespace wm
}  // namespace ash

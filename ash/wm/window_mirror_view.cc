// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include <algorithm>
#include <memory>

#include "ash/wm/widget_finder.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace wm {
namespace {

void EnsureAllChildrenAreVisible(ui::Layer* layer) {
  std::list<ui::Layer*> layers;
  layers.push_back(layer);
  while (!layers.empty()) {
    for (auto* child : layers.front()->children())
      layers.push_back(child);
    layers.front()->SetVisible(true);
    layers.pop_front();
  }
}

// Removes 1 instance of an element from a multiset.
void EraseFromList(std::vector<aura::Window*>* targets, aura::Window* target) {
  auto it = std::find(targets->begin(), targets->end(), target);
  if (it != targets->end())
    targets->erase(it);
}

void RemoveTargetWindowFromSource(aura::Window* target, aura::Window* source) {
  if (!target || !source)
    return;
  std::vector<aura::Window*>* target_window_list =
      source->GetProperty(aura::client::kMirrorWindowList);
  if (!target_window_list)
    return;
  EraseFromList(target_window_list, target);
}

}  // namespace

WindowMirrorView::WindowMirrorView(aura::Window* source,
                                   bool trilinear_filtering_on_init)
    : source_(source),
      trilinear_filtering_on_init_(trilinear_filtering_on_init) {
  DCHECK(source);
}

WindowMirrorView::~WindowMirrorView() {
  // Make sure |source_| has outlived |this|. See crbug.com/681207
  DCHECK(source_->layer());
  RemoveTargetWindowFromSource(target_, source_);
}

void WindowMirrorView::RecreateMirrorLayers() {
  if (layer_owner_)
    layer_owner_.reset();

  InitLayerOwner();
}

gfx::Size WindowMirrorView::CalculatePreferredSize() const {
  return GetClientAreaBounds().size();
}

void WindowMirrorView::Layout() {
  // If |layer_owner_| hasn't been initialized (|this| isn't on screen), no-op.
  if (!layer_owner_)
    return;

  // Position at 0, 0.
  GetMirrorLayer()->SetBounds(gfx::Rect(GetMirrorLayer()->bounds().size()));

  gfx::Transform transform;
  gfx::Rect client_area_bounds = GetClientAreaBounds();
  // Scale down if necessary.
  if (size() != source_->bounds().size()) {
    const float scale =
        width() / static_cast<float>(client_area_bounds.width());
    transform.Scale(scale, scale);
  }
  // Reposition such that the client area is the only part visible.
  transform.Translate(-client_area_bounds.x(), -client_area_bounds.y());
  GetMirrorLayer()->SetTransform(transform);
}

bool WindowMirrorView::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void WindowMirrorView::OnVisibleBoundsChanged() {
  if (!layer_owner_ && !GetVisibleBounds().IsEmpty())
    InitLayerOwner();
}

void WindowMirrorView::NativeViewHierarchyChanged() {
  View::NativeViewHierarchyChanged();
  DCHECK(GetWidget());
  UpdateSourceWindowProperty();
}

void WindowMirrorView::AddedToWidget() {
  UpdateSourceWindowProperty();
}

void WindowMirrorView::RemovedFromWidget() {
  RemoveTargetWindowFromSource(target_, source_);
  target_ = nullptr;
}

void WindowMirrorView::UpdateSourceWindowProperty() {
  DCHECK(GetWidget());
  std::vector<aura::Window*>* target_window_list =
      source_->GetProperty(aura::client::kMirrorWindowList);

  // Remove 1 instance of |target_| from the list.
  if (target_ && target_window_list)
    EraseFromList(target_window_list, target_);

  // Set and insert the new target window associated with this mirror view.
  target_ = GetWidget()->GetNativeWindow();
  target_->TrackOcclusionState();

  // Allocate new memory for |target_window_list| here because as soon as a
  // call is made to SetProperty, the previous memory will be deallocated.
  auto temp_list =
      target_window_list
          ? std::make_unique<std::vector<aura::Window*>>(*target_window_list)
          : std::make_unique<std::vector<aura::Window*>>();

  temp_list->push_back(target_);

  // Set the property to trigger a call to OnWindowPropertyChanged() on all the
  // window observer.
  // NOTE: This will deallocate the current property value so make sure new
  // memory has been allocated for the property value.
  source_->SetProperty(aura::client::kMirrorWindowList, temp_list.release());
}

void WindowMirrorView::InitLayerOwner() {
  layer_owner_ = ::wm::MirrorLayers(source_, false /* sync_bounds */);

  SetPaintToLayer();

  ui::Layer* mirror_layer = GetMirrorLayer();
  layer()->Add(mirror_layer);
  // This causes us to clip the non-client areas of the window.
  layer()->SetMasksToBounds(true);

  // Some extra work is needed when the source window is minimized.
  if (wm::GetWindowState(source_)->IsMinimized()) {
    mirror_layer->SetOpacity(1);
    EnsureAllChildrenAreVisible(mirror_layer);
  }

  if (trilinear_filtering_on_init_) {
    mirror_layer->AddCacheRenderSurfaceRequest();
    mirror_layer->AddTrilinearFilteringRequest();
  }

  Layout();
}

ui::Layer* WindowMirrorView::GetMirrorLayer() {
  return layer_owner_->root();
}

gfx::Rect WindowMirrorView::GetClientAreaBounds() const {
  int inset = source_->GetProperty(aura::client::kTopViewInset);
  if (inset > 0) {
    gfx::Rect bounds(source_->bounds().size());
    bounds.Inset(0, inset, 0, 0);
    return bounds;
  }
  // The source window may not have a widget in unit tests.
  views::Widget* widget = GetInternalWidgetForWindow(source_);
  if (!widget)
    return gfx::Rect();
  views::View* client_view = widget->client_view();
  return client_view->ConvertRectToWidget(client_view->GetLocalBounds());
}

}  // namespace wm
}  // namespace ash

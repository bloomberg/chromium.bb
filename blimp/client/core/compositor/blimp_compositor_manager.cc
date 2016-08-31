// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor_manager.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "cc/proto/compositor_message.pb.h"

namespace blimp {
namespace client {

namespace {
const int kDummyTabId = 0;
}  // namespace

BlimpCompositorManager::BlimpCompositorManager(
    RenderWidgetFeature* render_widget_feature,
    BlimpCompositorDependencies* compositor_dependencies)
    : render_widget_feature_(render_widget_feature),
      visible_(false),
      layer_(cc::Layer::Create()),
      active_compositor_(nullptr),
      compositor_dependencies_(compositor_dependencies) {
  DCHECK(render_widget_feature_);
  DCHECK(compositor_dependencies_);

  render_widget_feature_->SetDelegate(kDummyTabId, this);
}

BlimpCompositorManager::~BlimpCompositorManager() {
  render_widget_feature_->RemoveDelegate(kDummyTabId);
}

void BlimpCompositorManager::SetVisible(bool visible) {
  visible_ = visible;
  if (active_compositor_)
    active_compositor_->SetVisible(visible_);
}

bool BlimpCompositorManager::OnTouchEvent(const ui::MotionEvent& motion_event) {
  if (active_compositor_)
    return active_compositor_->OnTouchEvent(motion_event);
  return false;
}

std::unique_ptr<BlimpCompositor> BlimpCompositorManager::CreateBlimpCompositor(
    int render_widget_id,
    BlimpCompositorDependencies* compositor_dependencies,
    BlimpCompositorClient* client) {
  return base::MakeUnique<BlimpCompositor>(render_widget_id,
                                           compositor_dependencies, client);
}

void BlimpCompositorManager::OnRenderWidgetCreated(int render_widget_id) {
  CHECK(!GetCompositor(render_widget_id));

  compositors_[render_widget_id] =
      CreateBlimpCompositor(render_widget_id, compositor_dependencies_, this);
}

void BlimpCompositorManager::OnRenderWidgetInitialized(int render_widget_id) {
  if (active_compositor_ &&
      active_compositor_->render_widget_id() == render_widget_id)
    return;

  // Detach the content layer from the old compositor.
  layer_->RemoveAllChildren();

  if (active_compositor_) {
    VLOG(1) << "Hiding currently active compositor for render widget: "
            << active_compositor_->render_widget_id();
    active_compositor_->SetVisible(false);
  }

  active_compositor_ = GetCompositor(render_widget_id);
  CHECK(active_compositor_);

  active_compositor_->SetVisible(visible_);
  layer_->AddChild(active_compositor_->layer());
}

void BlimpCompositorManager::OnRenderWidgetDeleted(int render_widget_id) {
  CompositorMap::const_iterator it = compositors_.find(render_widget_id);
  CHECK(it != compositors_.end());

  // Reset the |active_compositor_| if that is what we're destroying right now.
  if (active_compositor_ == it->second.get()) {
    layer_->RemoveAllChildren();
    active_compositor_ = nullptr;
  }

  compositors_.erase(it);
}

void BlimpCompositorManager::OnCompositorMessageReceived(
    int render_widget_id,
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  BlimpCompositor* compositor = GetCompositor(render_widget_id);
  CHECK(compositor);

  compositor->OnCompositorMessageReceived(std::move(message));
}

void BlimpCompositorManager::SendWebGestureEvent(
    int render_widget_id,
    const blink::WebGestureEvent& gesture_event) {
  render_widget_feature_->SendWebGestureEvent(kDummyTabId, render_widget_id,
                                              gesture_event);
}

void BlimpCompositorManager::SendCompositorMessage(
    int render_widget_id,
    const cc::proto::CompositorMessage& message) {
  render_widget_feature_->SendCompositorMessage(kDummyTabId, render_widget_id,
                                                message);
}

BlimpCompositor* BlimpCompositorManager::GetCompositor(int render_widget_id) {
  CompositorMap::const_iterator it = compositors_.find(render_widget_id);
  if (it == compositors_.end())
    return nullptr;
  return it->second.get();
}

}  // namespace client
}  // namespace blimp

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_glow.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/threading/worker_pool.h"
#include "cc/layers/image_layer.h"
#include "content/browser/android/edge_effect.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/android/java_bitmap.h"

using std::max;
using std::min;

namespace content {

namespace {

const float kEpsilon = 1e-3f;

SkBitmap CreateSkBitmapFromAndroidResource(const char* name, gfx::Size size) {
  base::android::ScopedJavaLocalRef<jobject> jobj =
      gfx::CreateJavaBitmapFromAndroidResource(name, size);
  if (jobj.is_null())
    return SkBitmap();

  SkBitmap bitmap = CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(jobj.obj()));
  if (bitmap.isNull())
    return bitmap;

  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BOX, size.width(), size.height());
}

class OverscrollResources {
 public:
  OverscrollResources() {
    TRACE_EVENT0("browser", "OverscrollResources::Create");
    edge_bitmap_ =
        CreateSkBitmapFromAndroidResource("android:drawable/overscroll_edge",
                                          gfx::Size(128, 12));
    glow_bitmap_ =
        CreateSkBitmapFromAndroidResource("android:drawable/overscroll_glow",
                                          gfx::Size(128, 64));
  }

  const SkBitmap& edge_bitmap() { return edge_bitmap_; }
  const SkBitmap& glow_bitmap() { return glow_bitmap_; }

 private:
  SkBitmap edge_bitmap_;
  SkBitmap glow_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollResources);
};

// Leaky to allow access from a worker thread.
base::LazyInstance<OverscrollResources>::Leaky g_overscroll_resources =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<cc::Layer> CreateImageLayer(const SkBitmap& bitmap) {
  scoped_refptr<cc::ImageLayer> layer = cc::ImageLayer::Create();
  layer->SetBitmap(bitmap);
  return layer;
}

bool IsApproxZero(float value) {
  return std::abs(value) < kEpsilon;
}

gfx::Vector2dF ZeroSmallComponents(gfx::Vector2dF vector) {
  if (IsApproxZero(vector.x()))
    vector.set_x(0);
  if (IsApproxZero(vector.y()))
    vector.set_y(0);
  return vector;
}

// Force loading of any necessary resources.  This function is thread-safe.
void EnsureResources() {
  g_overscroll_resources.Get();
}

} // namespace

scoped_ptr<OverscrollGlow> OverscrollGlow::Create(bool enabled) {
  // Don't block the main thread with effect resource loading during creation.
  // Effect instantiation is deferred until the effect overscrolls, in which
  // case the main thread may block until the resource has loaded.
  if (enabled && g_overscroll_resources == NULL)
    base::WorkerPool::PostTask(FROM_HERE, base::Bind(EnsureResources), true);

  return make_scoped_ptr(new OverscrollGlow(enabled));
}

OverscrollGlow::OverscrollGlow(bool enabled)
    : enabled_(enabled), initialized_(false) {}

OverscrollGlow::~OverscrollGlow() {
  Detach();
}

void OverscrollGlow::Enable() {
  enabled_ = true;
}

void OverscrollGlow::Disable() {
  if (!enabled_)
    return;
  enabled_ = false;
  if (!enabled_ && initialized_) {
    Detach();
    for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i)
      edge_effects_[i]->Finish();
  }
}

bool OverscrollGlow::OnOverscrolled(cc::Layer* overscrolling_layer,
                                    base::TimeTicks current_time,
                                    gfx::Vector2dF overscroll,
                                    gfx::Vector2dF velocity) {
  DCHECK(overscrolling_layer);

  if (!enabled_)
    return false;

  // The size of the glow determines the relative effect of the inputs; an
  // empty-sized effect is effectively disabled.
  if (size_.IsEmpty())
    return false;

  // Ignore sufficiently small values that won't meaningfuly affect animation.
  overscroll = ZeroSmallComponents(overscroll);
  velocity = ZeroSmallComponents(velocity);

  if (overscroll.IsZero()) {
    if (initialized_) {
      Release(current_time);
      UpdateLayerAttachment(overscrolling_layer);
    }
    return NeedsAnimate();
  }

  if (!InitializeIfNecessary())
    return false;

  if (!velocity.IsZero()) {
    // Release effects if scrolling has changed directions.
    if (velocity.x() * old_velocity_.x() < 0)
      ReleaseAxis(AXIS_X, current_time);
    if (velocity.y() * old_velocity_.y() < 0)
      ReleaseAxis(AXIS_Y, current_time);

    Absorb(current_time, velocity, overscroll, old_overscroll_);
  } else {
    // Release effects when overscroll accumulation violates monotonicity.
    if (overscroll.x() * old_overscroll_.x() < 0 ||
        std::abs(overscroll.x()) < std::abs(old_overscroll_.x()))
      ReleaseAxis(AXIS_X, current_time);
    if (overscroll.y() * old_overscroll_.y() < 0 ||
        std::abs(overscroll.y()) < std::abs(old_overscroll_.y()))
      ReleaseAxis(AXIS_Y, current_time);

    Pull(current_time, overscroll - old_overscroll_);
  }

  old_velocity_ = velocity;
  old_overscroll_ = overscroll;

  UpdateLayerAttachment(overscrolling_layer);
  return NeedsAnimate();
}

bool OverscrollGlow::Animate(base::TimeTicks current_time) {
  if (!NeedsAnimate()) {
    Detach();
    return false;
  }

  const gfx::SizeF sizes[EdgeEffect::EDGE_COUNT] = {
    size_, gfx::SizeF(size_.height(), size_.width()),
    size_, gfx::SizeF(size_.height(), size_.width())
  };

  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    if (edge_effects_[i]->Update(current_time)) {
      edge_effects_[i]->ApplyToLayers(sizes[i],
                                      static_cast<EdgeEffect::Edge>(i));
    }
  }

  if (!NeedsAnimate()) {
    Detach();
    return false;
  }

  return true;
}

bool OverscrollGlow::NeedsAnimate() const {
  if (!enabled_ || !initialized_)
    return false;
  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    if (!edge_effects_[i]->IsFinished())
      return true;
  }
  return false;
}

void OverscrollGlow::UpdateLayerAttachment(cc::Layer* parent) {
  DCHECK(parent);
  if (!root_layer_)
    return;

  if (!NeedsAnimate()) {
    Detach();
    return;
  }

  if (root_layer_->parent() != parent)
    parent->AddChild(root_layer_);
}

void OverscrollGlow::Detach() {
  if (root_layer_)
    root_layer_->RemoveFromParent();
}

bool OverscrollGlow::InitializeIfNecessary() {
  DCHECK(enabled_);
  if (initialized_)
    return true;

  const SkBitmap& edge = g_overscroll_resources.Get().edge_bitmap();
  const SkBitmap& glow = g_overscroll_resources.Get().glow_bitmap();
  if (edge.isNull() || glow.isNull()) {
    Disable();
    return false;
  }

  DCHECK(!root_layer_);
  root_layer_ = cc::Layer::Create();
  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    scoped_refptr<cc::Layer> edge_layer = CreateImageLayer(edge);
    scoped_refptr<cc::Layer> glow_layer = CreateImageLayer(glow);
    root_layer_->AddChild(edge_layer);
    root_layer_->AddChild(glow_layer);
    edge_effects_[i] = make_scoped_ptr(new EdgeEffect(edge_layer, glow_layer));
  }

  initialized_ = true;
  return true;
}

void OverscrollGlow::Pull(base::TimeTicks current_time,
                          gfx::Vector2dF overscroll_delta) {
  DCHECK(enabled_ && initialized_);
  overscroll_delta = ZeroSmallComponents(overscroll_delta);
  if (overscroll_delta.IsZero())
    return;

  gfx::Vector2dF overscroll_pull = gfx::ScaleVector2d(overscroll_delta,
                                                      1.f / size_.width(),
                                                      1.f / size_.height());
  float edge_overscroll_pull[EdgeEffect::EDGE_COUNT] = {
    min(overscroll_pull.y(), 0.f), // Top
    min(overscroll_pull.x(), 0.f), // Left
    max(overscroll_pull.y(), 0.f), // Bottom
    max(overscroll_pull.x(), 0.f)  // Right
  };

  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    if (!edge_overscroll_pull[i])
      continue;

    edge_effects_[i]->Pull(current_time, std::abs(edge_overscroll_pull[i]));
    GetOppositeEdge(i)->Release(current_time);
  }
}

void OverscrollGlow::Absorb(base::TimeTicks current_time,
                            gfx::Vector2dF velocity,
                            gfx::Vector2dF overscroll,
                            gfx::Vector2dF old_overscroll) {
  DCHECK(enabled_ && initialized_);
  if (overscroll.IsZero() || velocity.IsZero())
    return;

  // Only trigger on initial overscroll at a non-zero velocity
  const float overscroll_velocities[EdgeEffect::EDGE_COUNT] = {
    old_overscroll.y() >= 0 && overscroll.y() < 0 ? min(velocity.y(), 0.f) : 0,
    old_overscroll.x() >= 0 && overscroll.x() < 0 ? min(velocity.x(), 0.f) : 0,
    old_overscroll.y() <= 0 && overscroll.y() > 0 ? max(velocity.y(), 0.f) : 0,
    old_overscroll.x() <= 0 && overscroll.x() > 0 ? max(velocity.x(), 0.f) : 0
  };

  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    if (!overscroll_velocities[i])
      continue;

    edge_effects_[i]->Absorb(current_time, std::abs(overscroll_velocities[i]));
    GetOppositeEdge(i)->Release(current_time);
  }
}

void OverscrollGlow::Release(base::TimeTicks current_time) {
  DCHECK(initialized_);
  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    edge_effects_[i]->Release(current_time);
  }
  old_overscroll_ = old_velocity_ = gfx::Vector2dF();
}

void OverscrollGlow::ReleaseAxis(Axis axis, base::TimeTicks current_time) {
  DCHECK(initialized_);
  switch (axis) {
    case AXIS_X:
      edge_effects_[EdgeEffect::EDGE_LEFT]->Release(current_time);
      edge_effects_[EdgeEffect::EDGE_RIGHT]->Release(current_time);
      old_overscroll_.set_x(0);
      old_velocity_.set_x(0);
      break;
    case AXIS_Y:
      edge_effects_[EdgeEffect::EDGE_TOP]->Release(current_time);
      edge_effects_[EdgeEffect::EDGE_BOTTOM]->Release(current_time);
      old_overscroll_.set_y(0);
      old_velocity_.set_y(0);
      break;
  };
}

EdgeEffect* OverscrollGlow::GetOppositeEdge(int edge_index) {
  DCHECK(initialized_);
  return edge_effects_[(edge_index + 2) % EdgeEffect::EDGE_COUNT].get();
}

}  // namespace content


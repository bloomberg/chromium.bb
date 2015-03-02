// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/composited_touch_handle_drawable.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/ui_resource_layer.h"
#include "jni/HandleViewResources_jni.h"
#include "ui/gfx/android/java_bitmap.h"

namespace content {

namespace {

static SkBitmap CreateSkBitmapFromJavaBitmap(
    base::android::ScopedJavaLocalRef<jobject> jbitmap) {
  return jbitmap.is_null()
             ? SkBitmap()
             : CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(jbitmap.obj()));
}

class HandleResources {
 public:
  HandleResources() : loaded_(false) {
  }

  void LoadIfNecessary(jobject context) {
    if (loaded_)
      return;

    loaded_ = true;

    TRACE_EVENT0("browser", "HandleResources::Create");
    JNIEnv* env = base::android::AttachCurrentThread();
    if (!context)
      context = base::android::GetApplicationContext();

    left_bitmap_ = CreateSkBitmapFromJavaBitmap(
        Java_HandleViewResources_getLeftHandleBitmap(env, context));
    right_bitmap_ = CreateSkBitmapFromJavaBitmap(
        Java_HandleViewResources_getRightHandleBitmap(env, context));
    center_bitmap_ = CreateSkBitmapFromJavaBitmap(
        Java_HandleViewResources_getCenterHandleBitmap(env, context));

    left_bitmap_.setImmutable();
    right_bitmap_.setImmutable();
    center_bitmap_.setImmutable();
  }

  const SkBitmap& GetBitmap(ui::TouchHandleOrientation orientation) {
    DCHECK(loaded_);
    switch (orientation) {
      case ui::TouchHandleOrientation::LEFT:
        return left_bitmap_;
      case ui::TouchHandleOrientation::RIGHT:
        return right_bitmap_;
      case ui::TouchHandleOrientation::CENTER:
        return center_bitmap_;
      case ui::TouchHandleOrientation::UNDEFINED:
        NOTREACHED() << "Invalid touch handle orientation.";
    };
    return center_bitmap_;
  }

 private:
  SkBitmap left_bitmap_;
  SkBitmap right_bitmap_;
  SkBitmap center_bitmap_;
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(HandleResources);
};

base::LazyInstance<HandleResources>::Leaky g_selection_resources;

}  // namespace

CompositedTouchHandleDrawable::CompositedTouchHandleDrawable(
    cc::Layer* root_layer,
    float dpi_scale,
    jobject context)
    : dpi_scale_(dpi_scale),
      orientation_(ui::TouchHandleOrientation::UNDEFINED),
      layer_(cc::UIResourceLayer::Create()) {
  g_selection_resources.Get().LoadIfNecessary(context);
  DCHECK(root_layer);
  root_layer->AddChild(layer_.get());
}

CompositedTouchHandleDrawable::~CompositedTouchHandleDrawable() {
  DetachLayer();
}

void CompositedTouchHandleDrawable::SetEnabled(bool enabled) {
  layer_->SetIsDrawable(enabled);
  // Force a position update in case the disabled layer's properties are stale.
  if (enabled)
    UpdateLayerPosition();
}

void CompositedTouchHandleDrawable::SetOrientation(
    ui::TouchHandleOrientation orientation) {
  DCHECK(layer_->parent());
  orientation_ = orientation;

  const SkBitmap& bitmap = g_selection_resources.Get().GetBitmap(orientation);
  layer_->SetBitmap(bitmap);
  layer_->SetBounds(gfx::Size(bitmap.width(), bitmap.height()));

  switch (orientation_) {
    case ui::TouchHandleOrientation::LEFT:
      focal_offset_from_origin_ = gfx::Vector2dF(bitmap.width() * 0.75f, 0);
      break;
    case ui::TouchHandleOrientation::RIGHT:
      focal_offset_from_origin_ = gfx::Vector2dF(bitmap.width() * 0.25f, 0);
      break;
    case ui::TouchHandleOrientation::CENTER:
      focal_offset_from_origin_ = gfx::Vector2dF(bitmap.width() * 0.5f, 0);
      break;
    case ui::TouchHandleOrientation::UNDEFINED:
      NOTREACHED() << "Invalid touch handle orientation.";
      break;
  };

  UpdateLayerPosition();
}

void CompositedTouchHandleDrawable::SetAlpha(float alpha) {
  DCHECK(layer_->parent());
  alpha = std::max(0.f, std::min(1.f, alpha));
  bool hidden = alpha <= 0;
  layer_->SetOpacity(alpha);
  layer_->SetHideLayerAndSubtree(hidden);
}

void CompositedTouchHandleDrawable::SetFocus(const gfx::PointF& position) {
  DCHECK(layer_->parent());
  focal_position_ = gfx::ScalePoint(position, dpi_scale_);
  UpdateLayerPosition();
}

gfx::RectF CompositedTouchHandleDrawable::GetVisibleBounds() const {
  return gfx::ScaleRect(gfx::RectF(layer_->position().x(),
                                   layer_->position().y(),
                                   layer_->bounds().width(),
                                   layer_->bounds().height()),
                        1.f / dpi_scale_);
}

void CompositedTouchHandleDrawable::DetachLayer() {
  layer_->RemoveFromParent();
}

void CompositedTouchHandleDrawable::UpdateLayerPosition() {
  layer_->SetPosition(focal_position_ - focal_offset_from_origin_);
}

// static
bool CompositedTouchHandleDrawable::RegisterHandleViewResources(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content

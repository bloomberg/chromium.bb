// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/composited_touch_handle_drawable.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
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

  const SkBitmap& GetBitmap(TouchHandleOrientation orientation) {
    DCHECK(loaded_);
    switch (orientation) {
      case TOUCH_HANDLE_LEFT:
        return left_bitmap_;
      case TOUCH_HANDLE_RIGHT:
        return right_bitmap_;
      case TOUCH_HANDLE_CENTER:
        return center_bitmap_;
      case TOUCH_HANDLE_ORIENTATION_UNDEFINED:
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
      orientation_(TOUCH_HANDLE_ORIENTATION_UNDEFINED),
      layer_(cc::UIResourceLayer::Create()) {
  g_selection_resources.Get().LoadIfNecessary(context);
  DCHECK(root_layer);
  root_layer->AddChild(layer_.get());
}

CompositedTouchHandleDrawable::~CompositedTouchHandleDrawable() {
  Detach();
}

void CompositedTouchHandleDrawable::SetEnabled(bool enabled) {
  layer_->SetIsDrawable(enabled);
}

void CompositedTouchHandleDrawable::SetOrientation(
    TouchHandleOrientation orientation) {
  DCHECK(layer_->parent());
  orientation_ = orientation;

  const SkBitmap& bitmap = g_selection_resources.Get().GetBitmap(orientation);
  layer_->SetBitmap(bitmap);
  layer_->SetBounds(gfx::Size(bitmap.width(), bitmap.height()));

  switch (orientation_) {
    case TOUCH_HANDLE_LEFT:
      focal_offset_from_origin_ = gfx::Vector2dF(bitmap.width() * 0.75f, 0);
      break;
    case TOUCH_HANDLE_RIGHT:
      focal_offset_from_origin_ = gfx::Vector2dF(bitmap.width() * 0.25f, 0);
      break;
    case TOUCH_HANDLE_CENTER:
      focal_offset_from_origin_ = gfx::Vector2dF(bitmap.width() * 0.5f, 0);
      break;
    case TOUCH_HANDLE_ORIENTATION_UNDEFINED:
      NOTREACHED() << "Invalid touch handle orientation.";
      break;
  };

  layer_->SetPosition(focal_position_ - focal_offset_from_origin_);
}

void CompositedTouchHandleDrawable::SetAlpha(float alpha) {
  DCHECK(layer_->parent());
  layer_->SetOpacity(std::max(0.f, std::min(1.f, alpha)));
}

void CompositedTouchHandleDrawable::SetFocus(const gfx::PointF& position) {
  DCHECK(layer_->parent());
  focal_position_ = gfx::ScalePoint(position, dpi_scale_);
  layer_->SetPosition(focal_position_ - focal_offset_from_origin_);
}

void CompositedTouchHandleDrawable::SetVisible(bool visible) {
  DCHECK(layer_->parent());
  layer_->SetHideLayerAndSubtree(!visible);
}

bool CompositedTouchHandleDrawable::IntersectsWith(
    const gfx::RectF& rect) const {
  return BoundingRect().Intersects(gfx::ScaleRect(rect, dpi_scale_));
}

void CompositedTouchHandleDrawable::Detach() {
  layer_->RemoveFromParent();
}

gfx::RectF CompositedTouchHandleDrawable::BoundingRect() const {
  return gfx::RectF(layer_->position().x(),
                    layer_->position().y(),
                    layer_->bounds().width(),
                    layer_->bounds().height());
}

// static
bool CompositedTouchHandleDrawable::RegisterHandleViewResources(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_android.h"

#include <android/native_window_jni.h>
#include "mojo/services/native_viewport/android/mojo_viewport.h"
#include "mojo/shell/context.h"
#include "ui/events/event.h"
#include "ui/gfx/point.h"

namespace mojo {
namespace services {

NativeViewportAndroid::NativeViewportAndroid(NativeViewportDelegate* delegate)
    : delegate_(delegate),
      window_(NULL),
      id_generator_(0),
      weak_factory_(this) {
}

NativeViewportAndroid::~NativeViewportAndroid() {
  if (window_)
    ReleaseWindow();
}

void NativeViewportAndroid::OnNativeWindowCreated(ANativeWindow* window) {
  DCHECK(!window_);
  window_ = window;
  delegate_->OnAcceleratedWidgetAvailable(window_);
}

void NativeViewportAndroid::OnNativeWindowDestroyed() {
  DCHECK(window_);
  ReleaseWindow();
}

void NativeViewportAndroid::OnResized(const gfx::Size& size) {
  size_ = size;
  delegate_->OnResized(size);
}

void NativeViewportAndroid::OnTouchEvent(int pointer_id,
                                         ui::EventType action,
                                         float x, float y,
                                         int64 time_ms) {
  gfx::Point location(static_cast<int>(x), static_cast<int>(y));
  ui::TouchEvent event(action, location,
                       id_generator_.GetGeneratedID(pointer_id),
                       base::TimeDelta::FromMilliseconds(time_ms));
  // TODO(beng): handle multiple touch-points.
  delegate_->OnEvent(&event);
  if (action == ui::ET_TOUCH_RELEASED)
    id_generator_.ReleaseNumber(pointer_id);
}

void NativeViewportAndroid::ReleaseWindow() {
  ANativeWindow_release(window_);
  window_ = NULL;
}

gfx::Size NativeViewportAndroid::GetSize() {
  return size_;
}

void NativeViewportAndroid::Close() {
  // TODO(beng): close activity containing MojoView?

  // TODO(beng): perform this in response to view destruction.
  delegate_->OnDestroyed();
}

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  scoped_ptr<NativeViewportAndroid> native_viewport(
      new NativeViewportAndroid(delegate));

  MojoViewportInit* init = new MojoViewportInit();
  init->ui_runner = context->task_runners()->ui_runner();
  init->native_viewport = native_viewport->GetWeakPtr();

  context->task_runners()->java_runner()->PostTask(FROM_HERE,
      base::Bind(MojoViewport::CreateForActivity,
                 context->activity(),
                 init));

   return scoped_ptr<NativeViewport>(native_viewport.Pass());
}

}  // namespace services
}  // namespace mojo

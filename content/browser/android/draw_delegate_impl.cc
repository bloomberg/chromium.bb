// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/draw_delegate_impl.h"

#include "base/memory/singleton.h"

namespace content {

// static
DrawDelegate* DrawDelegate::GetInstance() {
  return DrawDelegateImpl::GetInstance();
}

// static
DrawDelegateImpl* DrawDelegateImpl::GetInstance() {
  return Singleton<DrawDelegateImpl>::get();
}

DrawDelegateImpl::DrawDelegateImpl() {
}

DrawDelegateImpl::~DrawDelegateImpl() {
}

void DrawDelegateImpl::SetUpdateCallback(
    const SurfaceUpdatedCallback& callback) {
  draw_callback_ = callback;
}

void DrawDelegateImpl::SetBounds(const gfx::Size& size) {
  size_ = size;
}

void DrawDelegateImpl::OnSurfaceUpdated(
    uint64 texture, RenderWidgetHostView* view,
    const SurfacePresentedCallback& present_callback) {
  draw_callback_.Run(texture, view, present_callback);
}

}  // namespace content

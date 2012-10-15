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

void DrawDelegateImpl::SetBounds(const gfx::Size& size) {
  size_ = size;
}

}  // namespace content

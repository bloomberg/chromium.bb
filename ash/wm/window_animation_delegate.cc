// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animation_delegate.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::WindowAnimationDelegate*);

namespace ash {
namespace internal {

DEFINE_WINDOW_PROPERTY_KEY(WindowAnimationDelegate*,
                           kWindowAnimationDelegateKey, NULL);

// static
void WindowAnimationDelegate::SetDelegate(aura::Window* window,
                                          WindowAnimationDelegate* delegate) {
  window->SetProperty(kWindowAnimationDelegateKey, delegate);
}

// static
WindowAnimationDelegate* WindowAnimationDelegate::GetDelegate(
    aura::Window* window) {
  return window->GetProperty(kWindowAnimationDelegateKey);
}


}  // namespace internal
}  // namespace ash

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_H_
#define ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_H_

#include "athena/athena_export.h"
#include "ui/aura/window.h"

namespace athena {

// Interface for an ordered list of aura::Window objects.
class ATHENA_EXPORT WindowListProvider {
 public:
  virtual ~WindowListProvider() {}

  // Returns an ordered list of windows.
  virtual aura::Window::Windows GetWindowList() const = 0;
};

}  // namespace athena

#endif  // ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_H_

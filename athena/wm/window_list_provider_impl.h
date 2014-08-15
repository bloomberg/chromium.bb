// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_
#define ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_

#include "athena/wm/public/window_list_provider.h"

namespace athena {

// This implementation of the WindowListProviderImpl uses the same order as in
// the container window's stacking order.
class ATHENA_EXPORT WindowListProviderImpl : public WindowListProvider {
 public:
  explicit WindowListProviderImpl(aura::Window* container);
  virtual ~WindowListProviderImpl();

 private:
  // WindowListProvider:
  virtual aura::Window::Windows GetWindowList() const OVERRIDE;

  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(WindowListProviderImpl);
};

}  // namespace athena

#endif  // ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_

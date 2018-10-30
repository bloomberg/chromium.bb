// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/vr_module_delegate.h"

namespace device {

namespace {
// Storing the global delegate in a raw pointer - as opposed to e.g. an
// std::unique_ptr - to avoid adding a static initializer.
VrModuleDelegate* g_vr_module_delegate = nullptr;
}  // namespace

// static
VrModuleDelegate* VrModuleDelegate::Get() {
  return g_vr_module_delegate;
}

// static
void VrModuleDelegate::Set(std::unique_ptr<VrModuleDelegate> delegate) {
  if (g_vr_module_delegate) {
    delete g_vr_module_delegate;
  }
  g_vr_module_delegate = delegate.release();
}

}  // namespace device

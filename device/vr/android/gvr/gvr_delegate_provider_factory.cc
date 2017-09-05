// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"

namespace device {

namespace {
GvrDelegateProviderFactory* g_gvr_delegate_provider_factory = nullptr;
}  // namespace

// static
GvrDelegateProvider* GvrDelegateProviderFactory::Create() {
  if (!g_gvr_delegate_provider_factory)
    return nullptr;
  return g_gvr_delegate_provider_factory->CreateGvrDelegateProvider();
}

// static
void GvrDelegateProviderFactory::Install(GvrDelegateProviderFactory* f) {
  if (g_gvr_delegate_provider_factory == f)
    return;
  delete g_gvr_delegate_provider_factory;
  g_gvr_delegate_provider_factory = f;
}

}  // namespace device

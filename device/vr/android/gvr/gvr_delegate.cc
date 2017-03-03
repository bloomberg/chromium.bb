// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_delegate.h"

#include "base/logging.h"

namespace device {

base::Callback<GvrDelegateProvider*()> GvrDelegateProvider::delegate_provider_;

GvrDelegateProvider* GvrDelegateProvider::GetInstance() {
  if (delegate_provider_.is_null())
    return nullptr;
  return delegate_provider_.Run();
}

void GvrDelegateProvider::SetInstance(
    const base::Callback<GvrDelegateProvider*()>& provider_callback) {
  delegate_provider_ = provider_callback;
}

}  // namespace device

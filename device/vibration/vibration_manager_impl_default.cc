// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vibration/vibration_manager_impl.h"

#include "base/basictypes.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

namespace {

class VibrationManagerEmptyImpl : public VibrationManager {
 public:
  void Vibrate(int64 milliseconds) override {}
  void Cancel() override {}

 private:
  friend VibrationManagerImpl;

  explicit VibrationManagerEmptyImpl(
      mojo::InterfaceRequest<VibrationManager> request)
      : binding_(this, request.Pass()) {}
  ~VibrationManagerEmptyImpl() override {}

  // The binding between this object and the other end of the pipe.
  mojo::StrongBinding<VibrationManager> binding_;
};

}  // namespace

// static
void VibrationManagerImpl::Create(
    mojo::InterfaceRequest<VibrationManager> request) {
  new VibrationManagerEmptyImpl(request.Pass());
}

}  // namespace device

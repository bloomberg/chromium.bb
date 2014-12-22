// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vibration/vibration_manager_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"

namespace device {

namespace {

class VibrationManagerEmptyImpl : public mojo::InterfaceImpl<VibrationManager> {
 public:
  static VibrationManagerEmptyImpl* Create() {
    return new VibrationManagerEmptyImpl();
  }

  void Vibrate(int64 milliseconds) override {}
  void Cancel() override {}

 private:
  VibrationManagerEmptyImpl() {}
  ~VibrationManagerEmptyImpl() override {}
};

}  // namespace

//static
void VibrationManagerImpl::Create(
    mojo::InterfaceRequest<VibrationManager> request) {
  BindToRequest(VibrationManagerEmptyImpl::Create(), &request);
}

}  // namespace device

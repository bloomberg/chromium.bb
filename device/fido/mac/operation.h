// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_OPERATION_H_
#define DEVICE_FIDO_MAC_OPERATION_H_

#include "base/macros.h"

namespace device {
namespace fido {
namespace mac {

class Operation {
 public:
  Operation() = default;
  virtual ~Operation() = default;
  virtual void Run() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Operation);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_OPERATION_H_

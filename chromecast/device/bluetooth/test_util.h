// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_TEST_UTIL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_TEST_UTIL_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"

namespace chromecast {
namespace bluetooth {

class StatusCallbackChecker {
 public:
  StatusCallbackChecker();
  ~StatusCallbackChecker();

  void Reset();
  base::OnceCallback<void(bool success)> CreateCallback();

  bool called() const { return called_; }
  bool status() const { return status_; }

 private:
  void OnStatus(bool status);

  bool status_ = false;
  bool called_ = false;

  DISALLOW_COPY_AND_ASSIGN(StatusCallbackChecker);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_TEST_UTIL_H_

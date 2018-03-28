// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/test_util.h"

namespace chromecast {
namespace bluetooth {

StatusCallbackChecker::StatusCallbackChecker() = default;
StatusCallbackChecker::~StatusCallbackChecker() = default;

void StatusCallbackChecker::Reset() {
  called_ = false;
  status_ = false;
}

base::OnceCallback<void(bool success)> StatusCallbackChecker::CreateCallback() {
  return base::BindOnce(&StatusCallbackChecker::OnStatus,
                        base::Unretained(this));
}

void StatusCallbackChecker::OnStatus(bool status) {
  called_ = true;
  status_ = status;
}

}  // namespace bluetooth
}  // namespace chromecast

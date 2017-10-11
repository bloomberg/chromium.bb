// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/waiter.h"

namespace mojo {
namespace test {

Waiter::Waiter() = default;

Waiter::~Waiter() = default;

void Waiter::Wait() {
  loop_->Run();
}

}  // namespace test
}  // namespace mojo

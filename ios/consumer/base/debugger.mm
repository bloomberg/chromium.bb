// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "ios/public/consumer/base/debugger.h"

namespace ios {

bool BeingDebugged() {
  return base::debug::BeingDebugged();
}

}  // namespace ios

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "gtest_ppapi/gtest_module.h"

namespace pp {

Module* CreateModule() {
  return new GTestModule();
}

}  // namespace pp

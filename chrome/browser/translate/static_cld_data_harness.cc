// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "static_cld_data_harness.h"

namespace test {

StaticCldDataHarness::StaticCldDataHarness() {
  // static data requires no special work
}

StaticCldDataHarness::~StaticCldDataHarness() {
  // static data requires no special work
}

void StaticCldDataHarness::Init() {
  // static data requires no special work
}

scoped_ptr<CldDataHarness> CreateCldDataHarness() {
  scoped_ptr<CldDataHarness> result(new StaticCldDataHarness());
  return result.Pass();
}

}  // namespace test

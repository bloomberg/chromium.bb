// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_base.h"

#include "components/exo/test/exo_test_helper.h"

namespace exo {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// ExoTestBase, public:

ExoTestBase::ExoTestBase() : exo_test_helper_(new ExoTestHelper) {}

ExoTestBase::~ExoTestBase() {}

void ExoTestBase::SetUp() {
  AshTestBase::SetUp();
}

void ExoTestBase::TearDown() {
  AshTestBase::TearDown();
}

}  // namespace test
}  // namespace exo

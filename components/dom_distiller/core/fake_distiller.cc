// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/fake_distiller.h"

namespace dom_distiller {
namespace test {

MockDistillerFactory::MockDistillerFactory() {}
MockDistillerFactory::~MockDistillerFactory() {}

FakeDistiller::FakeDistiller() {
  EXPECT_CALL(*this, Die()).Times(testing::AnyNumber());
}

FakeDistiller::~FakeDistiller() { Die(); }

}  // namespace test
}  // namespace dom_distiller

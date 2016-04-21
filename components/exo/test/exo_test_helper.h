// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace exo {
namespace test {

// A helper class that does common initialization required for Exosphere.
class ExoTestHelper {
 public:
  ExoTestHelper();
  ~ExoTestHelper();

  // Creates a GpuMemoryBuffer instance that can be used for tests.
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExoTestHelper);
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_

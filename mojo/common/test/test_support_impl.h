// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_TEST_TEST_SUPPORT_IMPL_H_
#define MOJO_COMMON_TEST_TEST_SUPPORT_IMPL_H_

#include "base/macros.h"
#include "mojo/public/tests/test_support_private.h"

namespace mojo {
namespace test {

class TestSupportImpl : public TestSupport {
 public:
  TestSupportImpl();
  virtual ~TestSupportImpl();

  virtual void LogPerfResult(const char* test_name,
                             double value,
                             const char* units) OVERRIDE;
  virtual FILE* OpenSourceRootRelativeFile(const char* relative_path) OVERRIDE;
  virtual char** EnumerateSourceRootRelativeDirectory(const char* relative_path)
      OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSupportImpl);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_COMMON_TEST_TEST_SUPPORT_IMPL_H_

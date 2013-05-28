// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SUITE_H_
#define ASH_TEST_TEST_SUITE_H_

#include "base/compiler_specific.h"
#include "base/test/test_suite.h"

#if defined(OS_WIN)
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_com_initializer.h"
#endif

namespace ash {
namespace test {

class AuraShellTestSuite : public base::TestSuite {
 public:
  AuraShellTestSuite(int argc, char** argv);

 protected:
  // base::TestSuite:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

 private:
#if defined(OS_WIN)
  scoped_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SUITE_H_

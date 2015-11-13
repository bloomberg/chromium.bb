// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_SUITE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_SUITE_H_

#include "base/test/test_suite.h"
#include "mojo/public/cpp/system/macros.h"

namespace mus {

class WindowServerTestSuite : public base::TestSuite {
 public:
  WindowServerTestSuite(int argc, char** argv);
  ~WindowServerTestSuite() override;

 protected:
  void Initialize() override;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowServerTestSuite);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_SUITE_H_

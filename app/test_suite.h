// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_TEST_SUITE_H_
#define APP_TEST_SUITE_H_
#pragma once

#include "base/test/test_suite.h"

class AppTestSuite : public base::TestSuite {
 public:
  AppTestSuite(int argc, char** argv);
  virtual ~AppTestSuite();

 protected:
  virtual void Initialize();
  virtual void Shutdown();
};

#endif  // APP_TEST_SUITE_H_

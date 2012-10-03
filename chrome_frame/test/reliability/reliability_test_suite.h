// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_RELIABILITY_RELIABILITY_TEST_SUITE_H_
#define CHROME_FRAME_TEST_RELIABILITY_RELIABILITY_TEST_SUITE_H_

#include "base/win/scoped_com_initializer.h"
#include "chrome_frame/test/reliability/page_load_test.h"
#include "chrome/test/ui/ui_test_suite.h"

class ReliabilityTestSuite : public UITestSuite {
 public:
  ReliabilityTestSuite(int argc, char** argv) : UITestSuite(argc, argv) {
  }

 private:
  virtual void Initialize() OVERRIDE {
    com_initializer_.reset(new base::win::ScopedCOMInitializer(
        base::win::ScopedCOMInitializer::kMTA));
    SetPageRange(*CommandLine::ForCurrentProcess());
    UITestSuite::Initialize();
  }

  virtual void Shutdown() OVERRIDE {
    UITestSuite::Shutdown();
    com_initializer_.reset();
  }

  scoped_ptr<base::win::ScopedCOMInitializer> com_initializer_;

  DISALLOW_COPY_AND_ASSIGN(ReliabilityTestSuite);
};

#endif  // CHROME_FRAME_TEST_RELIABILITY_RELIABILITY_TEST_SUITE_H_


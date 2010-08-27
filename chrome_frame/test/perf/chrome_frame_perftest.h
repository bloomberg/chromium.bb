// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_PERF_CHROME_FRAME_PERFTEST_H_
#define CHROME_FRAME_TEST_PERF_CHROME_FRAME_PERFTEST_H_
#include <atlbase.h>
#include "base/perftimer.h"
#include "testing/gtest/include/gtest/gtest.h"

class SimpleModule : public CAtlExeModuleT<SimpleModule> {
 public:
  // The ATL code does not set _pAtlModule to NULL on destruction, and therefore
  // creating new module (for another test) will ASSERT in constructor.
  ~SimpleModule() {
    Term();
    _pAtlModule = NULL;
  }
};
#endif  // CHROME_FRAME_TEST_PERF_CHROME_FRAME_PERFTEST_H_

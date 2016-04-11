// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/unittest_test_suite.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

#if defined(USE_X11)
#include <X11/Xlib.h>
#endif

namespace content {

UnitTestTestSuite::UnitTestTestSuite(base::TestSuite* test_suite)
    : test_suite_(test_suite) {
#if defined(USE_X11)
  XInitThreads();
#endif
#if defined(USE_AURA)
  DCHECK(!aura::Env::GetInstanceDontCreate());
  env_ = aura::Env::CreateInstance();
#endif
  DCHECK(test_suite);
  blink_test_support_.reset(new TestBlinkWebUnitTestSupport);
}

UnitTestTestSuite::~UnitTestTestSuite() {
  blink_test_support_.reset();
#if defined(USE_AURA)
  env_.reset();
#endif
}

int UnitTestTestSuite::Run() {
  return test_suite_->Run();
}

}  // namespace content

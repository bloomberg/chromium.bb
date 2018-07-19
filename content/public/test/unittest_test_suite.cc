// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/unittest_test_suite.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "third_party/blink/public/web/blink.h"

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_suite_setup.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11.h"
#endif

#if defined(OS_FUCHSIA)
#include "ui/ozone/public/ozone_switches.h"
#endif

namespace content {

UnitTestTestSuite::UnitTestTestSuite(base::TestSuite* test_suite)
    : test_suite_(test_suite) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string enabled =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string disabled =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);
  feature_list_.InitFromCommandLine(enabled, disabled);

#if defined(OS_FUCHSIA)
  // Use headless ozone platform on Fuchsia by default.
  // TODO(crbug.com/865172): Remove this flag.
  if (!command_line->HasSwitch(switches::kOzonePlatform))
    command_line->AppendSwitchASCII(switches::kOzonePlatform, "headless");
#endif

#if defined(USE_X11)
  XInitThreads();
#endif
#if defined(USE_AURA)
  aura_test_suite_setup_ = std::make_unique<aura::AuraTestSuiteSetup>();
#endif
  DCHECK(test_suite);
  blink_test_support_.reset(new TestBlinkWebUnitTestSupport);
}

UnitTestTestSuite::~UnitTestTestSuite() {
  blink_test_support_.reset();
#if defined(USE_AURA)
  aura_test_suite_setup_.reset();
#endif
}

int UnitTestTestSuite::Run() {
  return test_suite_->Run();
}

}  // namespace content

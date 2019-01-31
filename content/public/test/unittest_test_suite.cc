// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/unittest_test_suite.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
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

  // Unit tests don't currently work with the Network Service enabled.
  // base::TestSuite will reset the FeatureList, so modify the underlying
  // CommandLine object to disable the network service when it's parsed again.
  disabled += ",NetworkService";
  base::CommandLine new_command_line(command_line->GetProgram());
  base::CommandLine::SwitchMap switches = command_line->GetSwitches();
  switches.erase(switches::kDisableFeatures);
  new_command_line.AppendSwitchASCII(switches::kDisableFeatures, disabled);
  for (const auto& iter : switches)
    new_command_line.AppendSwitchNative(iter.first, iter.second);
  *base::CommandLine::ForCurrentProcess() = new_command_line;

  // The TaskScheduler created by the test launcher is never destroyed.
  // Similarly, the FeatureList created here is never destroyed so it
  // can safely be accessed by the TaskScheduler.
  std::unique_ptr<base::FeatureList> feature_list =
      std::make_unique<base::FeatureList>();
  feature_list->InitializeFromCommandLine(enabled, disabled);
  base::FeatureList::SetInstance(std::move(feature_list));

#if defined(OS_FUCHSIA)
  // Use headless ozone platform on Fuchsia by default.
  // TODO(crbug.com/865172): Remove this flag.
  command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kOzonePlatform))
    command_line->AppendSwitchASCII(switches::kOzonePlatform, "headless");
#endif

#if defined(USE_X11)
  XInitThreads();
#endif
  DCHECK(test_suite);
  blink_test_support_.reset(new TestBlinkWebUnitTestSupport);
}

UnitTestTestSuite::~UnitTestTestSuite() = default;

int UnitTestTestSuite::Run() {
#if defined(USE_AURA)
  // Must be initialized after test suites manipulate feature flags.
  aura::AuraTestSuiteSetup aura_setup;
#endif

  return test_suite_->Run();
}

}  // namespace content

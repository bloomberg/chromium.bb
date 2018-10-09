// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "base/run_loop.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

static int RunHelper(base::TestSuite* test_suite) {
#if defined(USE_OZONE)
  base::MessageLoopForUI main_loop;
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  params.using_mojo = true;
  ui::OzonePlatform::InitializeForGPU(params);
  std::vector<gl::GLImplementation> allowed_impls =
      gl::init::GetAllowedGLImplementations();
  DCHECK(!allowed_impls.empty());
#else
  base::MessageLoopForIO message_loop;
#endif
  base::FeatureList::InitializeInstance(std::string(), std::string());

  CHECK(gl::init::InitializeGLOneOff());
  return test_suite->Run();
}

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  base::CommandLine::Init(argc, argv);

  // Always run the perf tests serially, to avoid distorting
  // perf measurements with randomness resulting from running
  // in parallel.
  const auto& run_test_suite =
      base::Bind(&RunHelper, base::Unretained(&test_suite));
  return base::LaunchUnitTestsSerially(argc, argv, run_test_suite);
}

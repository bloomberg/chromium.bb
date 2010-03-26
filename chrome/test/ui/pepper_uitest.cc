// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/npapi_test_helper.h"

#if defined(OS_WIN)
static const char kPepperTestPluginName[] = "pepper_test_plugin.dll";
#elif defined(OS_MACOSX)
static const char kPepperTestPluginName[] = "PepperTestPlugin.plugin";
#elif defined(OS_LINUX)
static const char kPepperTestPluginName[] = "libpepper_test_plugin.so";
#endif

using npapi_test::kTestCompleteCookie;
using npapi_test::kTestCompleteSuccess;
using npapi_test::kLongWaitTimeout;
using npapi_test::kShortWaitTimeout;

// Helper class pepper NPAPI tests.
class PepperTester : public NPAPITesterBase {
 protected:
  PepperTester() : NPAPITesterBase(kPepperTestPluginName) {}

  virtual void SetUp() {
    launch_arguments_.AppendSwitch(switches::kInternalPepper);
    launch_arguments_.AppendSwitch(switches::kEnableGPUPlugin);
    NPAPITesterBase::SetUp();
  }
};

// Test that a pepper 3d plugin loads and renders.
// TODO(alokp): Enable the test after making sure it works on all platforms
// and buildbots have OpenGL support.
TEST_F(PepperTester, DISABLED_Pepper3D) {
  GURL url = GetTestUrl(L"pepper", L"pepper_3d.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("pepper_3d", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kLongWaitTimeout);
}

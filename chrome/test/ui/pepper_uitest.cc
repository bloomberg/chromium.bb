// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"

#include "app/app_switches.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "chrome/test/ui_test_utils.h"

#if defined(OS_WIN)
static const char kPepperTestPluginName[] = "npapi_pepper_test_plugin.dll";
#elif defined(OS_MACOSX)
static const char kPepperTestPluginName[] = "npapi_pepper_test_plugin.plugin";
#elif defined(OS_LINUX)
static const char kPepperTestPluginName[] = "libnpapi_pepper_test_plugin.so";
#endif

using npapi_test::kTestCompleteCookie;
using npapi_test::kTestCompleteSuccess;

// Helper class pepper NPAPI tests.
class PepperTester : public NPAPITesterBase {
 protected:
  PepperTester() : NPAPITesterBase(kPepperTestPluginName) {}

  virtual void SetUp() {
    // TODO(alokp): Remove no-sandbox flag once gpu plugin can run in sandbox.
    launch_arguments_.AppendSwitch(switches::kNoSandbox);
    launch_arguments_.AppendSwitch(switches::kInternalPepper);
    launch_arguments_.AppendSwitch(switches::kEnableGPUPlugin);
    // Use Mesa software renderer so it can run on testbots without any
    // graphics hardware.
    launch_arguments_.AppendSwitchWithValue(switches::kUseGL, "osmesa");
    NPAPITesterBase::SetUp();
  }
};

// Test that a pepper 3d plugin loads and renders.
// TODO(alokp): Enable the test after making sure it works on all platforms
// and buildbots have OpenGL support.
#if defined(OS_WIN)
// Marked as FAILS (46662): failing on buildbots but passes on trybots.
TEST_F(PepperTester, FAILS_Pepper3D) {
  const FilePath dir(FILE_PATH_LITERAL("pepper"));
  const FilePath file(FILE_PATH_LITERAL("pepper_3d.html"));
  GURL url = ui_test_utils::GetTestUrl(dir, file);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("pepper_3d", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}
#endif

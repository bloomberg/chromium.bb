// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/ppapi/ppapi_test.h"
#include "ppapi/shared_impl/test_harness_utils.h"

// This file lists tests for Pepper APIs (without NaCl) against content_shell.
// TODO(teravest): Move more tests here. http://crbug.com/371873

// See chrome/test/ppapi/ppapi_browsertests.cc for Pepper testing that's
// covered in browser_tests.

namespace content {
namespace {

// This macro finesses macro expansion to do what we want.
#define STRIP_PREFIXES(test_name) ppapi::StripTestPrefixes(#test_name)

#define TEST_PPAPI_IN_PROCESS(test_name) \
  IN_PROC_BROWSER_TEST_F(PPAPITest, test_name) { \
    RunTest(STRIP_PREFIXES(test_name)); \
  }

#define TEST_PPAPI_OUT_OF_PROCESS(test_name) \
  IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
    RunTest(STRIP_PREFIXES(test_name)); \
  }

TEST_PPAPI_IN_PROCESS(BrowserFont)
// crbug.com/308949
#if defined(OS_WIN)
#define MAYBE_OUT_BrowserFont DISABLED_BrowserFont
#else
#define MAYBE_OUT_BrowserFont BrowserFont
#endif
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_OUT_BrowserFont)

TEST_PPAPI_IN_PROCESS(Buffer)
TEST_PPAPI_OUT_OF_PROCESS(Buffer)

TEST_PPAPI_IN_PROCESS(CharSet)
TEST_PPAPI_OUT_OF_PROCESS(CharSet)

TEST_PPAPI_IN_PROCESS(Console)
TEST_PPAPI_OUT_OF_PROCESS(Console)

TEST_PPAPI_IN_PROCESS(Core)
TEST_PPAPI_OUT_OF_PROCESS(Core)

TEST_PPAPI_IN_PROCESS(Crypto)
TEST_PPAPI_OUT_OF_PROCESS(Crypto)

TEST_PPAPI_IN_PROCESS(Graphics2D)
TEST_PPAPI_OUT_OF_PROCESS(Graphics2D)

TEST_PPAPI_IN_PROCESS(ImageData)
TEST_PPAPI_OUT_OF_PROCESS(ImageData)

TEST_PPAPI_OUT_OF_PROCESS(InputEvent)

// "Instance" tests are really InstancePrivate tests. InstancePrivate is not
// supported in NaCl, so these tests are only run trusted.
// Also note that these tests are run separately on purpose (versus collapsed
// in to one IN_PROC_BROWSER_TEST_F macro), because some of them have leaks
// on purpose that will look like failures to tests that are run later.
TEST_PPAPI_IN_PROCESS(Instance_ExecuteScript)
TEST_PPAPI_OUT_OF_PROCESS(Instance_ExecuteScript)

IN_PROC_BROWSER_TEST_F(PPAPITest,
                       Instance_ExecuteScriptAtInstanceShutdown) {
  // In other tests, we use one call to RunTest so that the tests can all run
  // in one plugin instance. This saves time on loading the plugin (especially
  // for NaCl). Here, we actually want to destroy the Instance, to test whether
  // the destructor can run ExecuteScript successfully. That's why we have two
  // separate calls to RunTest; the second one forces a navigation which
  // destroys the instance from the prior RunTest.
  // See test_instance_deprecated.cc for more information.
  RunTest("Instance_SetupExecuteScriptAtInstanceShutdown");
  RunTest("Instance_ExecuteScriptAtInstanceShutdown");
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest,
                       Instance_ExecuteScriptAtInstanceShutdown) {
  // (See the comment for the in-process version of this test above)
  RunTest("Instance_SetupExecuteScriptAtInstanceShutdown");
  RunTest("Instance_ExecuteScriptAtInstanceShutdown");
}

TEST_PPAPI_IN_PROCESS(Instance_LeakedObjectDestructors)
TEST_PPAPI_OUT_OF_PROCESS(Instance_LeakedObjectDestructors)

// We run and reload the RecursiveObjects test to ensure that the
// InstanceObject (and others) are properly cleaned up after the first run.
IN_PROC_BROWSER_TEST_F(PPAPITest, Instance_RecursiveObjects) {
  RunTestAndReload("Instance_RecursiveObjects");
}
// TODO(dmichael): Make it work out-of-process (or at least see whether we
// care).
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest,
                       DISABLED_Instance_RecursiveObjects) {
  RunTestAndReload("Instance_RecursiveObjects");
}

TEST_PPAPI_OUT_OF_PROCESS(MediaStreamAudioTrack)

TEST_PPAPI_OUT_OF_PROCESS(MediaStreamVideoTrack)

TEST_PPAPI_IN_PROCESS(Memory)
TEST_PPAPI_OUT_OF_PROCESS(Memory)

TEST_PPAPI_OUT_OF_PROCESS(MessageHandler)

TEST_PPAPI_OUT_OF_PROCESS(MessageLoop_Basics)
TEST_PPAPI_OUT_OF_PROCESS(MessageLoop_Post)

TEST_PPAPI_OUT_OF_PROCESS(NetworkProxy)

// TODO(danakj): http://crbug.com/115286
TEST_PPAPI_IN_PROCESS(DISABLED_Scrollbar)
// http://crbug.com/89961
TEST_PPAPI_OUT_OF_PROCESS(DISABLED_Scrollbar)

TEST_PPAPI_IN_PROCESS(TraceEvent)
TEST_PPAPI_OUT_OF_PROCESS(TraceEvent)

TEST_PPAPI_OUT_OF_PROCESS(TrueTypeFont)

TEST_PPAPI_IN_PROCESS(URLUtil)
TEST_PPAPI_OUT_OF_PROCESS(URLUtil)

TEST_PPAPI_IN_PROCESS(Var)
TEST_PPAPI_OUT_OF_PROCESS(Var)

// Flaky on mac, http://crbug.com/121107
#if defined(OS_MACOSX)
#define MAYBE_VarDeprecated DISABLED_VarDeprecated
#else
#define MAYBE_VarDeprecated VarDeprecated
#endif
TEST_PPAPI_IN_PROCESS(VarDeprecated)
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_VarDeprecated)

TEST_PPAPI_IN_PROCESS(VarResource)
TEST_PPAPI_OUT_OF_PROCESS(VarResource)

TEST_PPAPI_OUT_OF_PROCESS(VideoDecoder)

TEST_PPAPI_IN_PROCESS(VideoDecoderDev)
TEST_PPAPI_OUT_OF_PROCESS(VideoDecoderDev)

}  // namespace
}  // namespace content

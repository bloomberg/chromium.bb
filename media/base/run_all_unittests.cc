// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/base/android/media_jni_registrar.h"
#endif

class TestSuiteNoAtExit : public base::TestSuite {
 public:
  TestSuiteNoAtExit(int argc, char** argv) : TestSuite(argc, argv) {}
  virtual ~TestSuiteNoAtExit() {}
 protected:
  virtual void Initialize() OVERRIDE;
};

void TestSuiteNoAtExit::Initialize() {
  // Run TestSuite::Initialize first so that logging is initialized.
  base::TestSuite::Initialize();

#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  JNIEnv* env = base::android::AttachCurrentThread();
  media::RegisterJni(env);
#endif

  // Run this here instead of main() to ensure an AtExitManager is already
  // present.
  media::InitializeMediaLibraryForTesting();
  // Enable VP9 video codec support for all media tests.
  // TODO(tomfinegan): Remove this once the VP9 flag is removed or negated.
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kEnableVp9Playback);
}

int main(int argc, char** argv) {
  return TestSuiteNoAtExit(argc, argv).Run();
}

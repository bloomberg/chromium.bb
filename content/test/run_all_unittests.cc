// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "build/build_config.h"
#include "content/app/mojo/mojo_init.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/test/content_test_suite.h"
#include "mojo/edk/test/scoped_ipc_support.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/test/test_file_util.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  base::RegisterContentUriTestUtils(base::android::AttachCurrentThread());
#endif
  content::UnitTestTestSuite test_suite(
      new content::ContentTestSuite(argc, argv));
#if !defined(OS_IOS)
  content::InitializeMojo();
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  scoped_ptr<mojo::edk::test::ScopedIPCSupport> ipc_support;
  ipc_support.reset(
      new mojo::edk::test::ScopedIPCSupport(test_io_thread.task_runner()));
#endif

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&content::UnitTestTestSuite::Run,
                             base::Unretained(&test_suite)));
}

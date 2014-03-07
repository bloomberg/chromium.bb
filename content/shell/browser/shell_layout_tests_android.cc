// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_layout_tests_android.h"

#include "base/android/fifo_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "content/public/test/nested_message_pump_android.h"
#include "content/shell/common/shell_switches.h"
#include "jni/ShellLayoutTestUtils_jni.h"
#include "url/gurl.h"

namespace {

// Path to search for when translating a layout test path to an URL.
const char kAndroidLayoutTestPath[] =
    "/data/local/tmp/third_party/WebKit/LayoutTests/";

// The base URL from which layout tests are being served on Android.
const char kAndroidLayoutTestBase[] = "http://127.0.0.1:8000/all-tests/";

base::FilePath GetTestFilesDirectory(JNIEnv* env) {
  ScopedJavaLocalRef<jstring> directory =
      content::Java_ShellLayoutTestUtils_getApplicationFilesDirectory(
          env, base::android::GetApplicationContext());
  return base::FilePath(ConvertJavaStringToUTF8(directory));
}

void EnsureCreateFIFO(const base::FilePath& path) {
  unlink(path.value().c_str());
  CHECK(base::android::CreateFIFO(path, 0666))
    << "Unable to create the Android's FIFO: " << path.value().c_str();
}

scoped_ptr<base::MessagePump> CreateMessagePumpForUI() {
  return scoped_ptr<base::MessagePump>(new content::NestedMessagePumpAndroid());
}

}  // namespace

namespace content {

bool GetTestUrlForAndroid(std::string& path_or_url, GURL* url) {
  if (path_or_url.find(kAndroidLayoutTestPath) == std::string::npos)
    return false;

  std::string test_location(kAndroidLayoutTestBase);
  test_location.append(path_or_url.substr(strlen(kAndroidLayoutTestPath)));

  *url = GURL(test_location);
  return true;
}

void EnsureInitializeForAndroidLayoutTests() {
  CHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree));

  JNIEnv* env = base::android::AttachCurrentThread();
  content::NestedMessagePumpAndroid::RegisterJni(env);
  content::RegisterNativesImpl(env);

  bool success = base::MessageLoop::InitMessagePumpForUIFactory(
      &CreateMessagePumpForUI);
  CHECK(success) << "Unable to initialize the message pump for Android.";

  // Android will need three FIFOs to communicate with the Blink test runner,
  // one for each of [stdout, stderr, stdin].
  base::FilePath files_dir(GetTestFilesDirectory(env));

  base::FilePath stdout_fifo(files_dir.Append(FILE_PATH_LITERAL("test.fifo")));
  EnsureCreateFIFO(stdout_fifo);

  base::FilePath stderr_fifo(
      files_dir.Append(FILE_PATH_LITERAL("stderr.fifo")));
  EnsureCreateFIFO(stderr_fifo);

  base::FilePath stdin_fifo(files_dir.Append(FILE_PATH_LITERAL("stdin.fifo")));
  EnsureCreateFIFO(stdin_fifo);

  // Redirecting stdout needs to happen before redirecting stdin, which needs
  // to happen before redirecting stderr.
  success = base::android::RedirectStream(stdout, stdout_fifo, "w") &&
            base::android::RedirectStream(stdin, stdin_fifo, "r") &&
            base::android::RedirectStream(stderr, stderr_fifo, "w");

  CHECK(success) << "Unable to initialize the Android FIFOs.";
}

}  // namespace content

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/webview_instrumentation_test_native_jni/MemoryMetricsLoggerTest_jni.h"

#include "android_webview/browser/memory_metrics_logger.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"

namespace android_webview {

// static
jboolean JNI_MemoryMetricsLoggerTest_ForceRecordHistograms(JNIEnv* env) {
  auto* memory_metrics_logger = MemoryMetricsLogger::GetInstanceForTesting();
  if (!memory_metrics_logger)
    return false;

  TestTimeouts::Initialize();
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY);
  base::RunLoop run_loop;
  bool result = false;
  memory_metrics_logger->ScheduleRecordForTesting(
      base::BindLambdaForTesting([&](bool success) {
        result = success;
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

}  // namespace android_webview

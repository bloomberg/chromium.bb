// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

using base::debug::CategoryFilter;
using base::debug::TraceOptions;
using base::debug::RECORD_CONTINUOUSLY;
using base::debug::RECORD_UNTIL_FULL;

namespace content {

class TracingControllerTest : public ContentBrowserTest {
 public:
  TracingControllerTest() {}

  virtual void SetUp() OVERRIDE {
    get_categories_done_callback_count_ = 0;
    enable_recording_done_callback_count_ = 0;
    disable_recording_done_callback_count_ = 0;
    enable_monitoring_done_callback_count_ = 0;
    disable_monitoring_done_callback_count_ = 0;
    capture_monitoring_snapshot_done_callback_count_ = 0;
    ContentBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    ContentBrowserTest::TearDown();
  }

  void Navigate(Shell* shell) {
    NavigateToURL(shell, GetTestUrl("", "title.html"));
  }

  void GetCategoriesDoneCallbackTest(base::Closure quit_callback,
                                     const std::set<std::string>& categories) {
    get_categories_done_callback_count_++;
    EXPECT_TRUE(categories.size() > 0);
    quit_callback.Run();
  }

  void EnableRecordingDoneCallbackTest(base::Closure quit_callback) {
    enable_recording_done_callback_count_++;
    quit_callback.Run();
  }

  void DisableRecordingStringDoneCallbackTest(base::Closure quit_callback,
                                              base::RefCountedString* data) {
    disable_recording_done_callback_count_++;
    EXPECT_TRUE(data->size() > 0);
    quit_callback.Run();
  }

  void DisableRecordingFileDoneCallbackTest(base::Closure quit_callback,
                                            const base::FilePath& file_path) {
    disable_recording_done_callback_count_++;
    EXPECT_TRUE(PathExists(file_path));
    int64 file_size;
    base::GetFileSize(file_path, &file_size);
    EXPECT_TRUE(file_size > 0);
    quit_callback.Run();
    last_actual_recording_file_path_ = file_path;
  }

  void EnableMonitoringDoneCallbackTest(base::Closure quit_callback) {
    enable_monitoring_done_callback_count_++;
    quit_callback.Run();
  }

  void DisableMonitoringDoneCallbackTest(base::Closure quit_callback) {
    disable_monitoring_done_callback_count_++;
    quit_callback.Run();
  }

  void CaptureMonitoringSnapshotDoneCallbackTest(
      base::Closure quit_callback, const base::FilePath& file_path) {
    capture_monitoring_snapshot_done_callback_count_++;
    EXPECT_TRUE(PathExists(file_path));
    int64 file_size;
    base::GetFileSize(file_path, &file_size);
    EXPECT_TRUE(file_size > 0);
    quit_callback.Run();
    last_actual_monitoring_file_path_ = file_path;
  }

  int get_categories_done_callback_count() const {
    return get_categories_done_callback_count_;
  }

  int enable_recording_done_callback_count() const {
    return enable_recording_done_callback_count_;
  }

  int disable_recording_done_callback_count() const {
    return disable_recording_done_callback_count_;
  }

  int enable_monitoring_done_callback_count() const {
    return enable_monitoring_done_callback_count_;
  }

  int disable_monitoring_done_callback_count() const {
    return disable_monitoring_done_callback_count_;
  }

  int capture_monitoring_snapshot_done_callback_count() const {
    return capture_monitoring_snapshot_done_callback_count_;
  }

  base::FilePath last_actual_recording_file_path() const {
    return last_actual_recording_file_path_;
  }

  base::FilePath last_actual_monitoring_file_path() const {
    return last_actual_monitoring_file_path_;
  }

  void TestEnableAndDisableRecordingString() {
    Navigate(shell());

    TracingController* controller = TracingController::GetInstance();

    {
      base::RunLoop run_loop;
      TracingController::EnableRecordingDoneCallback callback =
          base::Bind(&TracingControllerTest::EnableRecordingDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());
      bool result = controller->EnableRecording(
          CategoryFilter(), TraceOptions(), callback);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_recording_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      base::Callback<void(base::RefCountedString*)> callback = base::Bind(
          &TracingControllerTest::DisableRecordingStringDoneCallbackTest,
          base::Unretained(this),
          run_loop.QuitClosure());
      bool result = controller->DisableRecording(
          TracingController::CreateStringSink(callback));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }
  }

  void TestEnableAndDisableRecordingFile(
      const base::FilePath& result_file_path) {
    Navigate(shell());

    TracingController* controller = TracingController::GetInstance();

    {
      base::RunLoop run_loop;
      TracingController::EnableRecordingDoneCallback callback =
          base::Bind(&TracingControllerTest::EnableRecordingDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());
      bool result = controller->EnableRecording(
          CategoryFilter(), TraceOptions(), callback);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_recording_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      base::Closure callback = base::Bind(
          &TracingControllerTest::DisableRecordingFileDoneCallbackTest,
          base::Unretained(this),
          run_loop.QuitClosure(),
          result_file_path);
      bool result = controller->DisableRecording(
          TracingController::CreateFileSink(result_file_path, callback));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }
  }

  void TestEnableCaptureAndDisableMonitoring(
      const base::FilePath& result_file_path) {
    Navigate(shell());

    TracingController* controller = TracingController::GetInstance();

    {
      bool is_monitoring;
      CategoryFilter category_filter("");
      TraceOptions options;
      controller->GetMonitoringStatus(
          &is_monitoring, &category_filter, &options);
      EXPECT_FALSE(is_monitoring);
      EXPECT_EQ("-*Debug,-*Test", category_filter.ToString());
      EXPECT_FALSE(options.record_mode == RECORD_CONTINUOUSLY);
      EXPECT_FALSE(options.enable_sampling);
      EXPECT_FALSE(options.enable_systrace);
    }

    {
      base::RunLoop run_loop;
      TracingController::EnableMonitoringDoneCallback callback =
          base::Bind(&TracingControllerTest::EnableMonitoringDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());

      TraceOptions trace_options;
      trace_options.enable_sampling = true;

      bool result = controller->EnableMonitoring(
          CategoryFilter("*"),
          trace_options,
          callback);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_monitoring_done_callback_count(), 1);
    }

    {
      bool is_monitoring;
      CategoryFilter category_filter("");
      TraceOptions options;
      controller->GetMonitoringStatus(
          &is_monitoring, &category_filter, &options);
      EXPECT_TRUE(is_monitoring);
      EXPECT_EQ("*", category_filter.ToString());
      EXPECT_FALSE(options.record_mode == RECORD_CONTINUOUSLY);
      EXPECT_TRUE(options.enable_sampling);
      EXPECT_FALSE(options.enable_systrace);
    }

    {
      base::RunLoop run_loop;
      base::Closure callback = base::Bind(
          &TracingControllerTest::CaptureMonitoringSnapshotDoneCallbackTest,
          base::Unretained(this),
          run_loop.QuitClosure(),
          result_file_path);
      ASSERT_TRUE(controller->CaptureMonitoringSnapshot(
          TracingController::CreateFileSink(result_file_path, callback)));
      run_loop.Run();
      EXPECT_EQ(capture_monitoring_snapshot_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      TracingController::DisableMonitoringDoneCallback callback =
          base::Bind(&TracingControllerTest::DisableMonitoringDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());
      bool result = controller->DisableMonitoring(callback);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_monitoring_done_callback_count(), 1);
    }

    {
      bool is_monitoring;
      CategoryFilter category_filter("");
      TraceOptions options;
      controller->GetMonitoringStatus(&is_monitoring,
                                      &category_filter,
                                      &options);
      EXPECT_FALSE(is_monitoring);
      EXPECT_EQ("", category_filter.ToString());
      EXPECT_FALSE(options.record_mode == RECORD_CONTINUOUSLY);
      EXPECT_FALSE(options.enable_sampling);
      EXPECT_FALSE(options.enable_systrace);
    }
  }

 private:
  int get_categories_done_callback_count_;
  int enable_recording_done_callback_count_;
  int disable_recording_done_callback_count_;
  int enable_monitoring_done_callback_count_;
  int disable_monitoring_done_callback_count_;
  int capture_monitoring_snapshot_done_callback_count_;
  base::FilePath last_actual_recording_file_path_;
  base::FilePath last_actual_monitoring_file_path_;
};

IN_PROC_BROWSER_TEST_F(TracingControllerTest, GetCategories) {
  Navigate(shell());

  TracingController* controller = TracingController::GetInstance();

  base::RunLoop run_loop;
  TracingController::GetCategoriesDoneCallback callback =
      base::Bind(&TracingControllerTest::GetCategoriesDoneCallbackTest,
                 base::Unretained(this),
                 run_loop.QuitClosure());
  ASSERT_TRUE(controller->GetCategories(callback));
  run_loop.Run();
  EXPECT_EQ(get_categories_done_callback_count(), 1);
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, EnableAndDisableRecording) {
  TestEnableAndDisableRecordingString();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableAndDisableRecordingWithFilePath) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  TestEnableAndDisableRecordingFile(file_path);
  EXPECT_EQ(file_path.value(), last_actual_recording_file_path().value());
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableAndDisableRecordingWithEmptyFileAndNullCallback) {
  Navigate(shell());

  TracingController* controller = TracingController::GetInstance();
  EXPECT_TRUE(controller->EnableRecording(
      CategoryFilter(),
      TraceOptions(),
      TracingController::EnableRecordingDoneCallback()));
  EXPECT_TRUE(controller->DisableRecording(NULL));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableCaptureAndDisableMonitoring) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  TestEnableCaptureAndDisableMonitoring(file_path);
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableCaptureAndDisableMonitoringWithFilePath) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  TestEnableCaptureAndDisableMonitoring(file_path);
  EXPECT_EQ(file_path.value(), last_actual_monitoring_file_path().value());
}

// See http://crbug.com/392446
#if defined(OS_ANDROID)
#define MAYBE_EnableCaptureAndDisableMonitoringWithEmptyFileAndNullCallback \
    DISABLED_EnableCaptureAndDisableMonitoringWithEmptyFileAndNullCallback
#else
#define MAYBE_EnableCaptureAndDisableMonitoringWithEmptyFileAndNullCallback \
    EnableCaptureAndDisableMonitoringWithEmptyFileAndNullCallback
#endif
IN_PROC_BROWSER_TEST_F(
    TracingControllerTest,
    MAYBE_EnableCaptureAndDisableMonitoringWithEmptyFileAndNullCallback) {
  Navigate(shell());

  TracingController* controller = TracingController::GetInstance();
  TraceOptions trace_options;
  trace_options.enable_sampling = true;
  EXPECT_TRUE(controller->EnableMonitoring(
      CategoryFilter("*"),
      trace_options,
      TracingController::EnableMonitoringDoneCallback()));
  controller->CaptureMonitoringSnapshot(NULL);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->DisableMonitoring(
      TracingController::DisableMonitoringDoneCallback()));
  base::RunLoop().RunUntilIdle();
}

}  // namespace content

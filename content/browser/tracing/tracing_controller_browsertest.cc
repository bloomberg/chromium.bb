// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/run_loop.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"

namespace content {

class TracingControllerTest : public ContentBrowserTest {
 public:
  TracingControllerTest() {}

  virtual void SetUp() OVERRIDE {
    get_categories_done_callback_count_ = 0;
    enable_recording_done_callback_count_ = 0;
    disable_recording_done_callback_count_ = 0;
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

  void DisableRecordingDoneCallbackTest(base::Closure quit_callback,
                                        scoped_ptr<base::FilePath> file_path) {
    disable_recording_done_callback_count_++;
    EXPECT_TRUE(PathExists(*file_path));
    quit_callback.Run();
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

 private:
  int get_categories_done_callback_count_;
  int enable_recording_done_callback_count_;
  int disable_recording_done_callback_count_;
};

IN_PROC_BROWSER_TEST_F(TracingControllerTest, GetCategories) {
  Navigate(shell());

  TracingController* controller = TracingController::GetInstance();

  base::RunLoop run_loop;
  TracingController::GetCategoriesDoneCallback callback =
      base::Bind(&TracingControllerTest::GetCategoriesDoneCallbackTest,
                 base::Unretained(this),
                 run_loop.QuitClosure());
  controller->GetCategories(callback);
  run_loop.Run();
  EXPECT_EQ(get_categories_done_callback_count(), 1);
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, EnableAndDisableRecording) {
  Navigate(shell());

  TracingController* controller = TracingController::GetInstance();

  {
    base::RunLoop run_loop;
    TracingController::EnableRecordingDoneCallback callback =
        base::Bind(&TracingControllerTest::EnableRecordingDoneCallbackTest,
                   base::Unretained(this),
                   run_loop.QuitClosure());
    controller->EnableRecording(base::debug::CategoryFilter("*"),
                                TracingController::Options(), callback);
    run_loop.Run();
    EXPECT_EQ(enable_recording_done_callback_count(), 1);
  }

  {
    base::RunLoop run_loop;
    TracingController::TracingFileResultCallback callback =
        base::Bind(&TracingControllerTest::DisableRecordingDoneCallbackTest,
                   base::Unretained(this),
                   run_loop.QuitClosure());
    controller->DisableRecording(callback);
    run_loop.Run();
    EXPECT_EQ(disable_recording_done_callback_count(), 1);
  }
}

}  // namespace content

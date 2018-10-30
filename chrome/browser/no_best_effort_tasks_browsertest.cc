// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class RunLoopUntilNonEmptyPaint : public content::WebContentsObserver {
 public:
  explicit RunLoopUntilNonEmptyPaint(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  ~RunLoopUntilNonEmptyPaint() override = default;

  // Runs a RunLoop on the main thread until the first non-empty frame is
  // painted for the WebContents provided to the constructor.
  void RunUntilNonEmptyPaint() {
    if (web_contents()->CompletedFirstVisuallyNonEmptyPaint())
      return;
    run_loop_.Run();
  }

 private:
  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RunLoopUntilNonEmptyPaint);
};

class NoBestEffortTasksTest : public InProcessBrowserTest {
 protected:
  NoBestEffortTasksTest() = default;
  ~NoBestEffortTasksTest() override = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableBackgroundTasks);
  }

  DISALLOW_COPY_AND_ASSIGN(NoBestEffortTasksTest);
};

}  // namespace

// Verify that it is possible to get the first non-empty paint without running
// BEST_EFFORT tasks.
//
// TODO(fdoray) https://crbug.com/833989:
// - Flaky timeout on ChromeOS ASAN
// - Consistent timeout on Win ASAN
#if defined(ADDRESS_SANITIZER) && (defined(OS_CHROMEOS) || defined(OS_WIN))
#define MAYBE_FirstNonEmptyPaintWithoutBestEffortTasks \
  DISABLED_FirstNonEmptyPaintWithoutBestEffortTasks
#else
#define MAYBE_FirstNonEmptyPaintWithoutBestEffortTasks \
  FirstNonEmptyPaintWithoutBestEffortTasks
#endif
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTest,
                       MAYBE_FirstNonEmptyPaintWithoutBestEffortTasks) {
  RunLoopUntilNonEmptyPaint run_loop_until_non_empty_paint(
      browser()->tab_strip_model()->GetActiveWebContents());
  run_loop_until_non_empty_paint.RunUntilNonEmptyPaint();
}

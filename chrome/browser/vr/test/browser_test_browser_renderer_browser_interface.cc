// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/browser_test_browser_renderer_browser_interface.h"
#include "base/bind.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/test/ui_utils.h"

namespace vr {

BrowserTestBrowserRendererBrowserInterface::
    BrowserTestBrowserRendererBrowserInterface(UiUtils* utils)
    : utils_(utils),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

BrowserTestBrowserRendererBrowserInterface::
    ~BrowserTestBrowserRendererBrowserInterface() = default;

void BrowserTestBrowserRendererBrowserInterface::ForceExitVr() {}

void BrowserTestBrowserRendererBrowserInterface::
    ReportUiOperationResultForTesting(const UiTestOperationType& action_type,
                                      const UiTestOperationResult& result) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UiUtils::ReportUiOperationResult,
                                base::Unretained(utils_), action_type, result));
}

}  // namespace vr

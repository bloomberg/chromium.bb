// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_UITEST_H_
#define CHROME_BROWSER_HISTORY_HISTORY_UITEST_H_
#pragma once

#include "base/file_path.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "ui/base/events.h"
#include "ui/gfx/rect.h"

class HistoryTester : public UITest {
 protected:
  HistoryTester() : UITest() { }

  virtual void SetUp() {
    show_window_ = true;
    UITest::SetUp();
  }
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_UITEST_H_

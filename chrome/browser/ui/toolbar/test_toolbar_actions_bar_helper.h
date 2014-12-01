// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTIONS_BAR_HELPER_H_
#define CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTIONS_BAR_HELPER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

class Browser;
class ToolbarActionsBar;

// A cross-platform container that creates and owns a ToolbarActionsBar and the
// corresponding view.
class TestToolbarActionsBarHelper {
 public:
  TestToolbarActionsBarHelper() {}
  virtual ~TestToolbarActionsBarHelper() {}

  static scoped_ptr<TestToolbarActionsBarHelper> Create(Browser* browser);

  virtual ToolbarActionsBar* GetToolbarActionsBar() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestToolbarActionsBarHelper);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTIONS_BAR_HELPER_H_

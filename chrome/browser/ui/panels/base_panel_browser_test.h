// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_
#define CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_
#pragma once

#include "base/task.h"
#include "base/values.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/auto_hiding_desktop_bar.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/gfx/rect.h"

class Panel;

class BasePanelBrowserTest : public InProcessBrowserTest {
 public:
  class MockAutoHidingDesktopBar : public AutoHidingDesktopBar {
   public:
    virtual ~MockAutoHidingDesktopBar() { }

    virtual void EnableAutoHiding(Alignment alignment,
                                  bool enabled,
                                  int thickness) = 0;
    virtual void SetVisibility(Alignment alignment, Visibility visibility) = 0;
    virtual void SetThickness(Alignment alignment, int thickness) = 0;
  };

  BasePanelBrowserTest();
  virtual ~BasePanelBrowserTest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

 protected:
  enum ShowFlag { SHOW_AS_ACTIVE, SHOW_AS_INACTIVE };

  struct CreatePanelParams {
    std::string name;
    gfx::Rect bounds;
    ShowFlag show_flag;
    GURL url;

    CreatePanelParams(const std::string& name,
                      const gfx::Rect& bounds,
                      ShowFlag show_flag)
        : name(name),
          bounds(bounds),
          show_flag(show_flag) {
    }
  };

  Panel* CreatePanelWithParams(const CreatePanelParams& params);
  Panel* CreatePanelWithBounds(const std::string& panel_name,
                               const gfx::Rect& bounds);
  Panel* CreatePanel(const std::string& panel_name);

  scoped_refptr<Extension> CreateExtension(const FilePath::StringType& path,
                                           Extension::Location location,
                                           const DictionaryValue& extra_value);

  gfx::Rect testing_work_area() const { return testing_work_area_; }

  MockAutoHidingDesktopBar* mock_auto_hiding_desktop_bar() const {
    return mock_auto_hiding_desktop_bar_.get();
  }

 private:

  gfx::Rect testing_work_area_;
  scoped_refptr<MockAutoHidingDesktopBar> mock_auto_hiding_desktop_bar_;
};

#endif  // CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_

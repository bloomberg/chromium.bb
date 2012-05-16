// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_
#define CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_
#pragma once

#include "base/values.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/gfx/rect.h"

class BasePanelBrowserTest : public InProcessBrowserTest {
 public:
  class MockDisplaySettingsProvider : public DisplaySettingsProvider {
   public:
    MockDisplaySettingsProvider() { }
    virtual ~MockDisplaySettingsProvider() { }

    virtual void SetPrimaryScreenArea(const gfx::Rect& primary_screen_area) = 0;
    virtual void SetWorkArea(const gfx::Rect& work_area) = 0;
    virtual void EnableAutoHidingDesktopBar(DesktopBarAlignment alignment,
                                            bool enabled,
                                            int thickness) = 0;
    virtual void SetDesktopBarVisibility(DesktopBarAlignment alignment,
                                         DesktopBarVisibility visibility) = 0;
    virtual void SetDesktopBarThickness(DesktopBarAlignment alignment,
                                        int thickness) = 0;
  };

  BasePanelBrowserTest();
  virtual ~BasePanelBrowserTest();

  // Linux bots use icewm which activate windows in ways that break
  // certain panel tests. Skip those tests when running on the bots.
  // We do not disable the tests to make it easy for developers to run
  // them locally.
  bool SkipTestIfIceWM();

  // Gnome running compiz refuses to activate a window that was initially
  // created as inactive, causing certain panel tests to fail. These tests
  // pass fine on the bots, but fail for developers as Gnome running compiz
  // is the typical linux dev machine configuration. We do not disable the
  // tests to ensure we still have coverage on the bots.
  bool SkipTestIfCompizWM();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

 protected:
  enum ActiveState { SHOW_AS_ACTIVE, SHOW_AS_INACTIVE };

  struct CreatePanelParams {
    std::string name;
    gfx::Rect bounds;
    ActiveState show_flag;
    GURL url;
    bool wait_for_fully_created;
    ActiveState expected_active_state;

    CreatePanelParams(const std::string& name,
                      const gfx::Rect& bounds,
                      ActiveState show_flag)
        : name(name),
          bounds(bounds),
          show_flag(show_flag),
          wait_for_fully_created(true),
          expected_active_state(show_flag) {
    }
  };

  Panel* CreatePanelWithParams(const CreatePanelParams& params);
  Panel* CreatePanelWithBounds(const std::string& panel_name,
                               const gfx::Rect& bounds);
  Panel* CreatePanel(const std::string& panel_name);

  Panel* CreateDockedPanel(const std::string& name, const gfx::Rect& bounds);
  Panel* CreateDetachedPanel(const std::string& name, const gfx::Rect& bounds);

  void WaitForPanelAdded(Panel* panel);
  void WaitForPanelRemoved(Panel* panel);
  void WaitForPanelActiveState(Panel* panel, ActiveState state);
  void WaitForWindowSizeAvailable(Panel* panel);
  void WaitForBoundsAnimationFinished(Panel* panel);
  void WaitForExpansionStateChanged(Panel* panel,
                                    Panel::ExpansionState expansion_state);

  void CreateTestTabContents(Browser* browser);

  scoped_refptr<Extension> CreateExtension(const FilePath::StringType& path,
                                           Extension::Location location,
                                           const DictionaryValue& extra_value);

  void MoveMouseAndWaitForExpansionStateChange(Panel* panel,
                                               const gfx::Point& position);
  static void MoveMouse(const gfx::Point& position);
  void CloseWindowAndWait(Browser* browser);
  static std::string MakePanelName(int index);

  // |primary_screen_area| must contain |work_area|. If empty rect is passed
  // to |work_area|, it will be set to same as |primary_screen_area|.
  void SetTestingAreas(const gfx::Rect& primary_screen_area,
                       const gfx::Rect& work_area);

  MockDisplaySettingsProvider* mock_display_settings_provider() const {
    return mock_display_settings_provider_;
  }

  // Some tests might not want to use the mock version.
  void disable_display_settings_mock() {
    mock_display_settings_enabled_ = false;
  }

  static const FilePath::CharType* kTestDir;

 private:
  // Passed to and owned by PanelManager.
  MockDisplaySettingsProvider* mock_display_settings_provider_;

  bool mock_display_settings_enabled_;
};

#endif  // CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_

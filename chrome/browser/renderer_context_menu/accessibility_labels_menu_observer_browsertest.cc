// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/accessibility_labels_menu_observer.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#else
#include "content/public/browser/browser_accessibility_state.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// A test class for accessibility labels. This test should be a browser test
// because it accesses resources.
class AccessibilityLabelsMenuObserverTest : public InProcessBrowserTest {
 public:
  AccessibilityLabelsMenuObserverTest();

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override { Reset(false); }

  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalAccessibilityLabels);
  }

  void Reset(bool incognito) {
    observer_.reset();
    menu_.reset(new MockRenderViewContextMenu(incognito));
    observer_.reset(new AccessibilityLabelsMenuObserver(menu_.get()));
    menu_->SetObserver(observer_.get());
  }

  void InitMenu() {
    content::ContextMenuParams params;
    observer_->InitMenu(params);
  }

  ~AccessibilityLabelsMenuObserverTest() override;
  MockRenderViewContextMenu* menu() { return menu_.get(); }
  AccessibilityLabelsMenuObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<AccessibilityLabelsMenuObserver> observer_;
  std::unique_ptr<MockRenderViewContextMenu> menu_;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityLabelsMenuObserverTest);
};

AccessibilityLabelsMenuObserverTest::AccessibilityLabelsMenuObserverTest() {}

AccessibilityLabelsMenuObserverTest::~AccessibilityLabelsMenuObserverTest() {}

}  // namespace

// Tests that opening a context menu does not show the menu option if a
// screen reader is not enabled, regardless of the image labels setting.
IN_PROC_BROWSER_TEST_F(AccessibilityLabelsMenuObserverTest,
                       AccessibilityLabelsNotShownWithoutScreenReader) {
  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled,
                                 false);
  InitMenu();
  EXPECT_EQ(0u, menu()->GetMenuSize());

  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled, true);
  InitMenu();
  EXPECT_EQ(0u, menu()->GetMenuSize());
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AccessibilityLabelsMenuObserverTest,
                       AccessibilityLabelsShowWithChromevoxEnabled) {
  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(true);
  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled,
                                 false);
  InitMenu();

  // Shows but is not checked.
  ASSERT_EQ(1u, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);

  Reset(false);
  // Shows and is checked when Chromevox and the setting are both on.
  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled, true);
  InitMenu();

  ASSERT_EQ(1u, menu()->GetMenuSize());
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.checked);
  EXPECT_FALSE(item.hidden);
}
#else
IN_PROC_BROWSER_TEST_F(AccessibilityLabelsMenuObserverTest,
                       AccessibilityLabelsShowWithScreenReaderEnabled) {
  // Spoof a screen reader.
  content::BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::AXMode::kScreenReader);
  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled,
                                 false);
  InitMenu();

  // Shows but is not checked.
  ASSERT_EQ(1u, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);

  Reset(false);
  // Shows and is checked when a screen reader and the setting are both on.
  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled, true);
  InitMenu();

  ASSERT_EQ(1u, menu()->GetMenuSize());
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.checked);
  EXPECT_FALSE(item.hidden);
}
#endif  // defined(OS_CHROMEOS)

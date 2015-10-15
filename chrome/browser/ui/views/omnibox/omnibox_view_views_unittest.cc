// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#endif

namespace {

class TestingOmniboxViewViews : public OmniboxViewViews {
 public:
  TestingOmniboxViewViews(OmniboxEditController* controller,
                          Profile* profile,
                          CommandUpdater* command_updater)
      : OmniboxViewViews(controller, profile, command_updater, false, NULL,
                         gfx::FontList()),
        update_popup_call_count_(0) {
  }

  void CheckUpdatePopupCallInfo(size_t call_count, const base::string16& text,
                                const gfx::Range& selection_range) {
    EXPECT_EQ(call_count, update_popup_call_count_);
    EXPECT_EQ(text, update_popup_text_);
    EXPECT_EQ(selection_range, update_popup_selection_range_);
  }

 private:
  // OmniboxView:
  void UpdatePopup() override {
    ++update_popup_call_count_;
    update_popup_text_ = text();
    update_popup_selection_range_ = GetSelectedRange();
  }

  size_t update_popup_call_count_;
  base::string16 update_popup_text_;
  gfx::Range update_popup_selection_range_;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxViewViews);
};

class TestingOmniboxEditController : public ChromeOmniboxEditController {
 public:
  explicit TestingOmniboxEditController(CommandUpdater* command_updater)
      : ChromeOmniboxEditController(command_updater) {}

 protected:
  // ChromeOmniboxEditController:
  void UpdateWithoutTabRestore() override {}
  void OnChanged() override {}
  void OnSetFocus() override {}
  void ShowURL() override {}
  ToolbarModel* GetToolbarModel() override { return nullptr; }
  const ToolbarModel* GetToolbarModel() const override { return nullptr; }
  content::WebContents* GetWebContents() override { return nullptr; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxEditController);
};

}  // namespace

class OmniboxViewViewsTest : public testing::Test {
 public:
  OmniboxViewViewsTest()
      : command_updater_(NULL),
        omnibox_edit_controller_(&command_updater_) {
  }

  TestingOmniboxViewViews* omnibox_view() {
    return omnibox_view_.get();
  }

  views::Textfield* omnibox_textfield() {
    return omnibox_view();
  }

 private:
  // testing::Test:
  void SetUp() override {
#if defined(OS_CHROMEOS)
    chromeos::input_method::InitializeForTesting(
        new chromeos::input_method::MockInputMethodManager);
#endif
    omnibox_view_.reset(new TestingOmniboxViewViews(
        &omnibox_edit_controller_, &profile_, &command_updater_));
    omnibox_view_->Init();
  }

  void TearDown() override {
    omnibox_view_.reset();
#if defined(OS_CHROMEOS)
    chromeos::input_method::Shutdown();
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  CommandUpdater command_updater_;
  TestingOmniboxEditController omnibox_edit_controller_;
  scoped_ptr<TestingOmniboxViewViews> omnibox_view_;
};

// Checks that a single change of the text in the omnibox invokes
// only one call to OmniboxViewViews::UpdatePopup().
TEST_F(OmniboxViewViewsTest, UpdatePopupCall) {
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::KEY_A, 0,
                          ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(
      1, base::ASCIIToUTF16("a"), gfx::Range(1));

  char_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::KEY_B, 0,
                   ui::DomKey::FromCharacter('b'), ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(
      2, base::ASCIIToUTF16("ab"), gfx::Range(2));

  ui::KeyEvent pressed(ui::ET_KEY_PRESSED, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyPressed(pressed);
  omnibox_view()->CheckUpdatePopupCallInfo(
      3, base::ASCIIToUTF16("a"), gfx::Range(1));
}

// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#endif

using gfx::Range;

namespace {

// TestingOmniboxView ---------------------------------------------------------

class TestingOmniboxView : public OmniboxViewViews {
 public:
  enum BaseTextEmphasis {
    DEEMPHASIZED,
    EMPHASIZED,
    UNSET,
  };

  TestingOmniboxView(OmniboxEditController* controller,
                     Profile* profile,
                     CommandUpdater* command_updater);

  static BaseTextEmphasis to_base_text_emphasis(bool emphasize) {
    return emphasize ? EMPHASIZED : DEEMPHASIZED;
  }

  void ResetEmphasisTestState();

  void CheckUpdatePopupCallInfo(size_t call_count,
                                const base::string16& text,
                                const Range& selection_range);

  Range scheme_range() const { return scheme_range_; }
  Range emphasis_range() const { return emphasis_range_; }
  BaseTextEmphasis base_text_emphasis() const { return base_text_emphasis_; }

  // OmniboxViewViews:
  void EmphasizeURLComponents() override;

 private:
  // OmniboxViewViews:
  // There is no popup and it doesn't actually matter whether we change the
  // visual style of the text, so these methods are all overridden merely to
  // capture relevant state at the time of the call, to be checked by test code.
  void UpdatePopup() override;
  void SetEmphasis(bool emphasize, const Range& range) override;
  void UpdateSchemeStyle(const Range& range) override;

  // Simplistic test override returns whether a given string looks like a URL
  // without having to mock AutocompleteClassifier objects and their
  // dependencies.
  bool CurrentTextIsURL() override {
    bool looks_like_url = (text().find(':') != std::string::npos ||
                           text().find('/') != std::string::npos);
    return looks_like_url;
  }

  size_t update_popup_call_count_ = 0;
  base::string16 update_popup_text_;
  Range update_popup_selection_range_;
  Profile* profile_;

  // Range of the last scheme logged by UpdateSchemeStyle().
  Range scheme_range_;

  // Range of the last text emphasized by SetEmphasis().
  Range emphasis_range_;

  // SetEmphasis() logs whether the base color of the text is emphasized.
  BaseTextEmphasis base_text_emphasis_ = UNSET;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxView);
};

TestingOmniboxView::TestingOmniboxView(OmniboxEditController* controller,
                                       Profile* profile,
                                       CommandUpdater* command_updater)
    : OmniboxViewViews(controller,
                       profile,
                       command_updater,
                       false,
                       nullptr,
                       gfx::FontList()),
      profile_(profile) {}

void TestingOmniboxView::ResetEmphasisTestState() {
  base_text_emphasis_ = UNSET;
  emphasis_range_ = Range::InvalidRange();
  scheme_range_ = Range::InvalidRange();
}

void TestingOmniboxView::CheckUpdatePopupCallInfo(
    size_t call_count,
    const base::string16& text,
    const Range& selection_range) {
  EXPECT_EQ(call_count, update_popup_call_count_);
  EXPECT_EQ(text, update_popup_text_);
  EXPECT_EQ(selection_range, update_popup_selection_range_);
}

void TestingOmniboxView::EmphasizeURLComponents() {
  UpdateTextStyle(text(), ChromeAutocompleteSchemeClassifier(profile_));
}

void TestingOmniboxView::UpdatePopup() {
  ++update_popup_call_count_;
  update_popup_text_ = text();
  update_popup_selection_range_ = GetSelectedRange();
}

void TestingOmniboxView::SetEmphasis(bool emphasize, const Range& range) {
  if (range == Range::InvalidRange()) {
    base_text_emphasis_ = to_base_text_emphasis(emphasize);
    return;
  }

  EXPECT_TRUE(emphasize);
  emphasis_range_ = range;
}

void TestingOmniboxView::UpdateSchemeStyle(const Range& range) {
  scheme_range_ = range;
}

// TestingOmniboxEditController -----------------------------------------------

class TestingOmniboxEditController : public ChromeOmniboxEditController {
 public:
  explicit TestingOmniboxEditController(CommandUpdater* command_updater)
      : ChromeOmniboxEditController(command_updater) {}

 private:
  // ChromeOmniboxEditController:
  void UpdateWithoutTabRestore() override {}
  void OnChanged() override {}
  ToolbarModel* GetToolbarModel() override { return nullptr; }
  const ToolbarModel* GetToolbarModel() const override { return nullptr; }
  content::WebContents* GetWebContents() override { return nullptr; }

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxEditController);
};

}  // namespace

// OmniboxViewViewsTest -------------------------------------------------------

class OmniboxViewViewsTest : public testing::Test {
 public:
  OmniboxViewViewsTest();

  TestingOmniboxView* omnibox_view() { return omnibox_view_.get(); }
  views::Textfield* omnibox_textfield() { return omnibox_view(); }
  ui::TextEditCommand scheduled_text_edit_command() const {
    return test_api_->scheduled_text_edit_command();
  }

  // Sets |new_text| as the omnibox text, and emphasizes it appropriately.
  void SetAndEmphasizeText(const std::string& new_text);

 private:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  CommandUpdater command_updater_;
  TestingOmniboxEditController omnibox_edit_controller_;
  std::unique_ptr<TestingOmniboxView> omnibox_view_;
  std::unique_ptr<views::TextfieldTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViewsTest);
};

OmniboxViewViewsTest::OmniboxViewViewsTest()
    : command_updater_(nullptr), omnibox_edit_controller_(&command_updater_) {}

void OmniboxViewViewsTest::SetAndEmphasizeText(const std::string& new_text) {
  omnibox_view()->ResetEmphasisTestState();
  omnibox_view()->SetText(base::ASCIIToUTF16(new_text));
  omnibox_view()->EmphasizeURLComponents();
}

void OmniboxViewViewsTest::SetUp() {
#if defined(OS_CHROMEOS)
  chromeos::input_method::InitializeForTesting(
      new chromeos::input_method::MockInputMethodManagerImpl);
#endif
  omnibox_view_ = base::MakeUnique<TestingOmniboxView>(
      &omnibox_edit_controller_, &profile_, &command_updater_);
  test_api_ = base::MakeUnique<views::TextfieldTestApi>(omnibox_view_.get());
  omnibox_view_->Init();
}

void OmniboxViewViewsTest::TearDown() {
  omnibox_view_.reset();
#if defined(OS_CHROMEOS)
  chromeos::input_method::Shutdown();
#endif
}

// Actual tests ---------------------------------------------------------------

// Checks that a single change of the text in the omnibox invokes
// only one call to OmniboxViewViews::UpdatePopup().
TEST_F(OmniboxViewViewsTest, UpdatePopupCall) {
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                          ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(1, base::ASCIIToUTF16("a"),
                                           Range(1));

  char_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::US_B, 0,
                   ui::DomKey::FromCharacter('b'), ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(2, base::ASCIIToUTF16("ab"),
                                           Range(2));

  ui::KeyEvent pressed(ui::ET_KEY_PRESSED, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyEvent(&pressed);
  omnibox_view()->CheckUpdatePopupCallInfo(3, base::ASCIIToUTF16("a"),
                                           Range(1));
}

// Test that the scheduled text edit command is cleared when Textfield receives
// a key press event. This ensures that the scheduled text edit command property
// is always in the correct state. Test for http://crbug.com/613948.
TEST_F(OmniboxViewViewsTest, ScheduledTextEditCommand) {
  omnibox_textfield()->SetTextEditCommandForNextKeyEvent(
      ui::TextEditCommand::MOVE_UP);
  EXPECT_EQ(ui::TextEditCommand::MOVE_UP, scheduled_text_edit_command());

  ui::KeyEvent up_pressed(ui::ET_KEY_PRESSED, ui::VKEY_UP, 0);
  omnibox_textfield()->OnKeyEvent(&up_pressed);
  EXPECT_EQ(ui::TextEditCommand::INVALID_COMMAND,
            scheduled_text_edit_command());
}

TEST_F(OmniboxViewViewsTest, Emphasis) {
  constexpr struct {
    const char* input;
    bool expected_base_text_emphasized;
    Range expected_emphasis_range;
    Range expected_scheme_range;
  } test_cases[] = {
      {"data:text/html,Hello%20World", false, Range(0, 4), Range(0, 4)},
      {"http://www.example.com/path/file.htm", false, Range(7, 22),
       Range(0, 4)},
      {"https://www.example.com/path/file.htm", false, Range(8, 23),
       Range(0, 5)},
      {"chrome-extension://ldfbacdbackkjhclmhnjabngnppnkagl", false,
       Range::InvalidRange(), Range(0, 16)},
      {"nosuchscheme://opaque/string", true, Range::InvalidRange(),
       Range(0, 12)},
      {"nosuchscheme:opaquestring", true, Range::InvalidRange(), Range(0, 12)},
      {"host.com/path/file", false, Range(0, 8), Range::InvalidRange()},
      {"This is plain text", true, Range::InvalidRange(),
       Range::InvalidRange()},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.input);

    SetAndEmphasizeText(test_case.input);
    EXPECT_EQ(TestingOmniboxView::to_base_text_emphasis(
                  test_case.expected_base_text_emphasized),
              omnibox_view()->base_text_emphasis());
    EXPECT_EQ(test_case.expected_emphasis_range,
              omnibox_view()->emphasis_range());
    EXPECT_EQ(test_case.expected_scheme_range, omnibox_view()->scheme_range());
  }
}

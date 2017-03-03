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

namespace {

class TestingOmniboxViewViews : public OmniboxViewViews {
 public:
  TestingOmniboxViewViews(OmniboxEditController* controller,
                          Profile* profile,
                          CommandUpdater* command_updater)
      : OmniboxViewViews(controller,
                         profile,
                         command_updater,
                         false,
                         nullptr,
                         gfx::FontList()),
        update_popup_call_count_(0),
        profile_(profile),
        base_text_is_emphasized_(false) {}

  void CheckUpdatePopupCallInfo(size_t call_count, const base::string16& text,
                                const gfx::Range& selection_range) {
    EXPECT_EQ(call_count, update_popup_call_count_);
    EXPECT_EQ(text, update_popup_text_);
    EXPECT_EQ(selection_range, update_popup_selection_range_);
  }

  void EmphasizeURLComponents() override {
    UpdateTextStyle(text(), ChromeAutocompleteSchemeClassifier(profile_));
  }

  gfx::Range scheme_range() const { return scheme_range_; }
  gfx::Range emphasis_range() const { return emphasis_range_; }
  bool base_text_is_emphasized() const { return base_text_is_emphasized_; }

 private:
  // OmniboxView:
  void UpdatePopup() override {
    ++update_popup_call_count_;
    update_popup_text_ = text();
    update_popup_selection_range_ = GetSelectedRange();
  }

  void SetEmphasis(bool emphasize, const gfx::Range& range) override {
    if (range == gfx::Range::InvalidRange()) {
      base_text_is_emphasized_ = emphasize;
      return;
    }

    EXPECT_TRUE(emphasize);
    emphasis_range_ = range;
  }

  void UpdateSchemeStyle(const gfx::Range& range) override {
    scheme_range_ = range;
  }

  // Simplistic test override returns whether a given string looks like a URL
  // without having to mock AutocompleteClassifier objects and their
  // dependencies.
  bool CurrentTextIsURL() override {
    bool looks_like_url = (text().find(':') != std::string::npos ||
                           text().find('/') != std::string::npos);
    return looks_like_url;
  }

  size_t update_popup_call_count_;
  base::string16 update_popup_text_;
  gfx::Range update_popup_selection_range_;
  Profile* profile_;

  // Range of the last scheme logged by UpdateSchemeStyle().
  gfx::Range scheme_range_;

  // Range of the last text emphasized by SetEmphasis().
  gfx::Range emphasis_range_;

  // SetEmphasis() logs whether the base color of the text is emphasized.
  bool base_text_is_emphasized_;

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

  ui::TextEditCommand scheduled_text_edit_command() const {
    return test_api_->scheduled_text_edit_command();
  }

  void SetAndEmphasizeText(const std::string& new_text) {
    omnibox_textfield()->SetText(base::ASCIIToUTF16(new_text));
    omnibox_view()->EmphasizeURLComponents();
  }

 private:
  // testing::Test:
  void SetUp() override {
#if defined(OS_CHROMEOS)
    chromeos::input_method::InitializeForTesting(
        new chromeos::input_method::MockInputMethodManagerImpl);
#endif
    omnibox_view_.reset(new TestingOmniboxViewViews(
        &omnibox_edit_controller_, &profile_, &command_updater_));
    test_api_.reset(new views::TextfieldTestApi(omnibox_view_.get()));
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
  std::unique_ptr<TestingOmniboxViewViews> omnibox_view_;
  std::unique_ptr<views::TextfieldTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViewsTest);
};

// Checks that a single change of the text in the omnibox invokes
// only one call to OmniboxViewViews::UpdatePopup().
TEST_F(OmniboxViewViewsTest, UpdatePopupCall) {
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                          ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(
      1, base::ASCIIToUTF16("a"), gfx::Range(1));

  char_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::US_B, 0,
                   ui::DomKey::FromCharacter('b'), ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(
      2, base::ASCIIToUTF16("ab"), gfx::Range(2));

  ui::KeyEvent pressed(ui::ET_KEY_PRESSED, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyEvent(&pressed);
  omnibox_view()->CheckUpdatePopupCallInfo(
      3, base::ASCIIToUTF16("a"), gfx::Range(1));
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

// Ensure that the scheme is emphasized for data: URLs.
TEST_F(OmniboxViewViewsTest, TestEmphasisForDATA) {
  SetAndEmphasizeText("data:text/html,Hello%20World");
  EXPECT_EQ(gfx::Range(0, 4), omnibox_view()->scheme_range());
  EXPECT_FALSE(omnibox_view()->base_text_is_emphasized());
  EXPECT_EQ(gfx::Range(0, 4), omnibox_view()->emphasis_range());
}

// Ensure that the origin is emphasized for http: URLs.
TEST_F(OmniboxViewViewsTest, TestEmphasisForHTTP) {
  SetAndEmphasizeText("http://www.example.com/path/file.htm");
  EXPECT_EQ(gfx::Range(0, 4), omnibox_view()->scheme_range());
  EXPECT_FALSE(omnibox_view()->base_text_is_emphasized());
  EXPECT_EQ(gfx::Range(7, 22), omnibox_view()->emphasis_range());
}

// Ensure that the origin is emphasized for https: URLs.
TEST_F(OmniboxViewViewsTest, TestEmphasisForHTTPS) {
  SetAndEmphasizeText("https://www.example.com/path/file.htm");
  EXPECT_EQ(gfx::Range(0, 5), omnibox_view()->scheme_range());
  EXPECT_FALSE(omnibox_view()->base_text_is_emphasized());
  EXPECT_EQ(gfx::Range(8, 23), omnibox_view()->emphasis_range());
}

// Ensure that nothing is emphasized for chrome-extension: URLs.
TEST_F(OmniboxViewViewsTest, TestEmphasisForChromeExtensionScheme) {
  SetAndEmphasizeText("chrome-extension://ldfbacdbackkjhclmhnjabngnppnkagl");
  EXPECT_EQ(gfx::Range(0, 16), omnibox_view()->scheme_range());
  EXPECT_FALSE(omnibox_view()->base_text_is_emphasized());
  EXPECT_EQ(gfx::Range(), omnibox_view()->emphasis_range());
}

// Ensure that everything is emphasized for unknown scheme hierarchical URLs.
TEST_F(OmniboxViewViewsTest, TestEmphasisForUnknownHierarchicalScheme) {
  SetAndEmphasizeText("nosuchscheme://opaque/string");
  EXPECT_EQ(gfx::Range(0, 12), omnibox_view()->scheme_range());
  EXPECT_TRUE(omnibox_view()->base_text_is_emphasized());
  EXPECT_EQ(gfx::Range(), omnibox_view()->emphasis_range());
}

// Ensure that everything is emphasized for unknown scheme URLs.
TEST_F(OmniboxViewViewsTest, TestEmphasisForUnknownScheme) {
  SetAndEmphasizeText("nosuchscheme:opaquestring");
  EXPECT_EQ(gfx::Range(0, 12), omnibox_view()->scheme_range());
  EXPECT_TRUE(omnibox_view()->base_text_is_emphasized());
  EXPECT_EQ(gfx::Range(), omnibox_view()->emphasis_range());
}

// Ensure that the origin is emphasized for URL-like text.
TEST_F(OmniboxViewViewsTest, TestEmphasisForPartialURLs) {
  SetAndEmphasizeText("example/path/file");
  EXPECT_EQ(gfx::Range(), omnibox_view()->scheme_range());
  EXPECT_FALSE(omnibox_view()->base_text_is_emphasized());
  EXPECT_EQ(gfx::Range(0, 7), omnibox_view()->emphasis_range());
}

// Ensure that everything is emphasized for plain text.
TEST_F(OmniboxViewViewsTest, TestEmphasisForNonURLs) {
  SetAndEmphasizeText("This is plain text");

  EXPECT_EQ(gfx::Range(), omnibox_view()->scheme_range());
  EXPECT_TRUE(omnibox_view()->base_text_is_emphasized());
  EXPECT_EQ(gfx::Range(), omnibox_view()->emphasis_range());
}

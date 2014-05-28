// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_cocoa.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/range/range.h"

using testing::AtLeast;
using testing::Return;

namespace {

class MockPasswordGenerationPopupController
   : public autofill::PasswordGenerationPopupController {
 public:
  MockPasswordGenerationPopupController()
    : help_text_(base::ASCIIToUTF16("Help me if you can I'm feeling dooown")),
      font_list_("Lucida Grande, 12px"),
      popup_bounds_(gfx::Rect(0, 0, 200, 100)) {}

  virtual void OnSavedPasswordsLinkClicked() OVERRIDE {}

  virtual const gfx::FontList& font_list() const OVERRIDE {
    return font_list_;
  }

  virtual const gfx::Rect& password_bounds() const OVERRIDE {
    return password_bounds_;
  }

  virtual const gfx::Rect& divider_bounds() const OVERRIDE {
    return divider_bounds_;
  }

  virtual const gfx::Rect& help_bounds() const OVERRIDE {
    return help_bounds_;
  }

  virtual bool display_password() const OVERRIDE { return true; }

  virtual bool password_selected() const OVERRIDE { return false; }

  MOCK_CONST_METHOD0(password, base::string16());

  virtual base::string16 SuggestedText() OVERRIDE {
    return base::ASCIIToUTF16("Suggested by Chrome");
  }

  virtual const base::string16& HelpText() OVERRIDE { return help_text_; }

  virtual const gfx::Range& HelpTextLinkRange() OVERRIDE { return link_range_; }

  // AutofillPopupViewDelegate implementation.
  virtual void Hide() OVERRIDE {}
  MOCK_METHOD0(ViewDestroyed, void());
  virtual void SetSelectionAtPoint(const gfx::Point&) OVERRIDE {}
  virtual bool AcceptSelectedLine() OVERRIDE { return true; }
  virtual void SelectionCleared() OVERRIDE {}
  virtual const gfx::Rect& popup_bounds() const OVERRIDE {
    return popup_bounds_;
  }
  MOCK_METHOD0(container_view, gfx::NativeView());

 private:
  base::string16 help_text_;
  const gfx::FontList font_list_;
  gfx::Range link_range_;

  gfx::Rect password_bounds_;
  gfx::Rect divider_bounds_;
  gfx::Rect help_bounds_;
  const gfx::Rect popup_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordGenerationPopupController);
};

class PasswordGenerationPopupViewCocoaTest : public CocoaTest {
 protected:
  PasswordGenerationPopupViewCocoaTest()
    : password_(base::ASCIIToUTF16("wow! such password"))
  {}

  virtual void SetUp() OVERRIDE {
    mock_controller_.reset(new MockPasswordGenerationPopupController);
    EXPECT_CALL(*mock_controller_, password())
        .WillRepeatedly(Return(password_));

    view_.reset([[PasswordGenerationPopupViewCocoa alloc]
        initWithController:mock_controller_.get()
                     frame:NSZeroRect]);

    NSView* contentView = [test_window() contentView];
    [contentView addSubview:view_];
    EXPECT_CALL(*mock_controller_, container_view())
        .WillRepeatedly(Return(contentView));
  }

  base::string16 password_;
  scoped_ptr<MockPasswordGenerationPopupController> mock_controller_;
  base::scoped_nsobject<PasswordGenerationPopupViewCocoa> view_;
};

TEST_VIEW(PasswordGenerationPopupViewCocoaTest, view_);

TEST_F(PasswordGenerationPopupViewCocoaTest, ShowAndHide) {
  // Verify that the view fetches a password from the controller.
  EXPECT_CALL(*mock_controller_, password()).Times(AtLeast(1))
      .WillRepeatedly(Return(password_));

  view_.reset([[PasswordGenerationPopupViewCocoa alloc]
      initWithController:mock_controller_.get()
                   frame:NSZeroRect]);

  [view_ showPopup];
  [view_ display];
  [view_ hidePopup];
}

// Verifies that it doesn't crash when the controller is destroyed before the
// popup is hidden.
TEST_F(PasswordGenerationPopupViewCocoaTest, ControllerDestroyed) {
  [view_ showPopup];
  mock_controller_.reset();
  [view_ controllerDestroyed];
  [view_ display];
  [view_ hidePopup];
}

}  // namespace

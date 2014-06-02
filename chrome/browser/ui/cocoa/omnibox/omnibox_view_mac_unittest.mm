// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"

#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/platform_test.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"

namespace {

class MockOmniboxEditModel : public OmniboxEditModel {
 public:
  MockOmniboxEditModel(OmniboxView* view,
                       OmniboxEditController* controller,
                       Profile* profile)
      : OmniboxEditModel(view, controller, profile),
        up_or_down_count_(0) {
  }

  virtual void OnUpOrDownKeyPressed(int count) OVERRIDE {
    up_or_down_count_ = count;
  }

  int up_or_down_count() const { return up_or_down_count_; }

  void set_up_or_down_count(int count) {
    up_or_down_count_ = count;
  }

 private:
  int up_or_down_count_;

  DISALLOW_COPY_AND_ASSIGN(MockOmniboxEditModel);
};

class MockOmniboxPopupView : public OmniboxPopupView {
 public:
  MockOmniboxPopupView() : is_open_(false) {}
  virtual ~MockOmniboxPopupView() {}

  // Overridden from OmniboxPopupView:
  virtual bool IsOpen() const OVERRIDE { return is_open_; }
  virtual void InvalidateLine(size_t line) OVERRIDE {}
  virtual void UpdatePopupAppearance() OVERRIDE {}
  virtual gfx::Rect GetTargetBounds() OVERRIDE { return gfx::Rect(); }
  virtual void PaintUpdatesNow() OVERRIDE {}
  virtual void OnDragCanceled() OVERRIDE {}

  void set_is_open(bool is_open) { is_open_ = is_open; }

 private:
  bool is_open_;

  DISALLOW_COPY_AND_ASSIGN(MockOmniboxPopupView);
};

class TestingToolbarModelDelegate : public ToolbarModelDelegate {
 public:
  TestingToolbarModelDelegate() {}
  virtual ~TestingToolbarModelDelegate() {}

  // Overridden from ToolbarModelDelegate:
  virtual content::WebContents* GetActiveWebContents() const OVERRIDE {
    return NULL;
  }
  virtual bool InTabbedBrowser() const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingToolbarModelDelegate);
};

class TestingOmniboxEditController : public OmniboxEditController {
 public:
  explicit TestingOmniboxEditController(ToolbarModel* toolbar_model)
      : OmniboxEditController(NULL),
        toolbar_model_(toolbar_model) {
  }
  virtual ~TestingOmniboxEditController() {}

 protected:
  // Overridden from OmniboxEditController:
  virtual void Update(const content::WebContents* contents) OVERRIDE {}
  virtual void OnChanged() OVERRIDE {}
  virtual void OnSetFocus() OVERRIDE {}
  virtual void ShowURL() OVERRIDE {}
  virtual void HideURL() OVERRIDE {}
  virtual void EndOriginChipAnimations(bool cancel_fade) OVERRIDE {}
  virtual InstantController* GetInstant() OVERRIDE { return NULL; }
  virtual content::WebContents* GetWebContents() OVERRIDE { return NULL; }
  virtual ToolbarModel* GetToolbarModel() OVERRIDE { return toolbar_model_; }
  virtual const ToolbarModel* GetToolbarModel() const OVERRIDE {
    return toolbar_model_;
  }

 private:
  ToolbarModel* toolbar_model_;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxEditController);
};

}  // namespace

class OmniboxViewMacTest : public CocoaProfileTest {
 public:
  void SetModel(OmniboxViewMac* view, OmniboxEditModel* model) {
    view->model_.reset(model);
  }
};

TEST_F(OmniboxViewMacTest, GetFieldFont) {
  EXPECT_TRUE(OmniboxViewMac::GetFieldFont(gfx::Font::NORMAL));
}

TEST_F(OmniboxViewMacTest, TabToAutocomplete) {
  OmniboxViewMac view(NULL, profile(), NULL, NULL);

  // This is deleted by the omnibox view.
  MockOmniboxEditModel* model =
      new MockOmniboxEditModel(&view, NULL, profile());
  SetModel(&view, model);

  MockOmniboxPopupView popup_view;
  OmniboxPopupModel popup_model(&popup_view, model);

  // With popup closed verify that tab doesn't autocomplete.
  popup_view.set_is_open(false);
  view.OnDoCommandBySelector(@selector(insertTab:));
  EXPECT_EQ(0, model->up_or_down_count());
  view.OnDoCommandBySelector(@selector(insertBacktab:));
  EXPECT_EQ(0, model->up_or_down_count());

  // With popup open verify that tab does autocomplete.
  popup_view.set_is_open(true);
  view.OnDoCommandBySelector(@selector(insertTab:));
  EXPECT_EQ(1, model->up_or_down_count());
  view.OnDoCommandBySelector(@selector(insertBacktab:));
  EXPECT_EQ(-1, model->up_or_down_count());
}

TEST_F(OmniboxViewMacTest, SetGrayTextAutocompletion) {
  const NSRect frame = NSMakeRect(0, 0, 50, 30);
  base::scoped_nsobject<AutocompleteTextField> field(
      [[AutocompleteTextField alloc] initWithFrame:frame]);

  TestingToolbarModelDelegate delegate;
  ToolbarModelImpl toolbar_model(&delegate);
  TestingOmniboxEditController controller(&toolbar_model);
  OmniboxViewMac view(&controller, profile(), NULL, field.get());

  // This is deleted by the omnibox view.
  MockOmniboxEditModel* model =
      new MockOmniboxEditModel(&view, &controller, profile());
  SetModel(&view, model);

  MockOmniboxPopupView popup_view;
  OmniboxPopupModel popup_model(&popup_view, model);

  view.SetUserText(base::ASCIIToUTF16("Alfred"));
  EXPECT_EQ("Alfred", base::UTF16ToUTF8(view.GetText()));
  view.SetGrayTextAutocompletion(base::ASCIIToUTF16(" Hitchcock"));
  EXPECT_EQ("Alfred", base::UTF16ToUTF8(view.GetText()));
  EXPECT_EQ(" Hitchcock", base::UTF16ToUTF8(view.GetGrayTextAutocompletion()));

  view.SetUserText(base::string16());
  EXPECT_EQ(base::string16(), view.GetText());
  EXPECT_EQ(base::string16(), view.GetGrayTextAutocompletion());
}

TEST_F(OmniboxViewMacTest, UpDownArrow) {
  OmniboxViewMac view(NULL, profile(), NULL, NULL);

  // This is deleted by the omnibox view.
  MockOmniboxEditModel* model =
      new MockOmniboxEditModel(&view, NULL, profile());
  SetModel(&view, model);

  MockOmniboxPopupView popup_view;
  OmniboxPopupModel popup_model(&popup_view, model);

  // With popup closed verify that pressing up and down arrow works.
  popup_view.set_is_open(false);
  model->set_up_or_down_count(0);
  view.OnDoCommandBySelector(@selector(moveDown:));
  EXPECT_EQ(1, model->up_or_down_count());
  model->set_up_or_down_count(0);
  view.OnDoCommandBySelector(@selector(moveUp:));
  EXPECT_EQ(-1, model->up_or_down_count());

  // With popup open verify that pressing up and down arrow works.
  popup_view.set_is_open(true);
  model->set_up_or_down_count(0);
  view.OnDoCommandBySelector(@selector(moveDown:));
  EXPECT_EQ(1, model->up_or_down_count());
  model->set_up_or_down_count(0);
  view.OnDoCommandBySelector(@selector(moveUp:));
  EXPECT_EQ(-1, model->up_or_down_count());
}

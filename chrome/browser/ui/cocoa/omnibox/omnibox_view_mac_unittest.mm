// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"

#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "testing/platform_test.h"

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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingToolbarModelDelegate);
};

class TestingOmniboxEditController : public OmniboxEditController {
 public:
  TestingOmniboxEditController() {}
  virtual ~TestingOmniboxEditController() {}

  // Overridden from OmniboxEditController:
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    content::PageTransition transition,
                                    const GURL& alternate_nav_url) OVERRIDE {}
  virtual void OnChanged() OVERRIDE {}
  virtual void OnSelectionBoundsChanged() OVERRIDE {}
  virtual void OnInputInProgress(bool in_progress) OVERRIDE {}
  virtual void OnKillFocus() OVERRIDE {}
  virtual void OnSetFocus() OVERRIDE {}
  virtual gfx::Image GetFavicon() const OVERRIDE { return gfx::Image(); }
  virtual string16 GetTitle() const OVERRIDE { return string16(); }
  virtual InstantController* GetInstant() OVERRIDE { return NULL; }
  virtual content::WebContents* GetWebContents() const OVERRIDE {
    return NULL;
  }

 private:
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
  EXPECT_TRUE(OmniboxViewMac::GetFieldFont());
}

TEST_F(OmniboxViewMacTest, TabToAutocomplete) {
  chrome::EnableInstantExtendedAPIForTesting();
  OmniboxViewMac view(NULL, NULL, profile(), NULL, NULL);

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

TEST_F(OmniboxViewMacTest, SetInstantSuggestion) {
  const NSRect frame = NSMakeRect(0, 0, 50, 30);
  scoped_nsobject<AutocompleteTextField> field(
      [[AutocompleteTextField alloc] initWithFrame:frame]);

  TestingToolbarModelDelegate delegate;
  ToolbarModelImpl toolbar_model(&delegate);
  OmniboxViewMac view(NULL, &toolbar_model, profile(), NULL, field.get());

  TestingOmniboxEditController controller;
  // This is deleted by the omnibox view.
  MockOmniboxEditModel* model =
      new MockOmniboxEditModel(&view, &controller, profile());
  SetModel(&view, model);

  MockOmniboxPopupView popup_view;
  OmniboxPopupModel popup_model(&popup_view, model);

  view.SetUserText(ASCIIToUTF16("Alfred"));
  EXPECT_EQ("Alfred", UTF16ToUTF8(view.GetText()));
  view.SetInstantSuggestion(ASCIIToUTF16(" Hitchcock"));
  EXPECT_EQ("Alfred", UTF16ToUTF8(view.GetText()));
  EXPECT_EQ(" Hitchcock", UTF16ToUTF8(view.GetInstantSuggestion()));

  view.SetUserText(string16());
  EXPECT_EQ(string16(), view.GetText());
  EXPECT_EQ(string16(), view.GetInstantSuggestion());
}

TEST_F(OmniboxViewMacTest, UpDownArrow) {
  OmniboxViewMac view(NULL, NULL, profile(), NULL, NULL);

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

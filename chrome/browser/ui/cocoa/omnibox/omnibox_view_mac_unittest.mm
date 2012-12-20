// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"

#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/search/search.h"
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
  chrome::search::EnableInstantExtendedAPIForTesting();
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

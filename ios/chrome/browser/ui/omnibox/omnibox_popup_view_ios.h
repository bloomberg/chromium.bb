// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_view_controller.h"

class OmniboxEditModel;
@class OmniboxPopupViewController;
class OmniboxPopupModel;
class OmniboxPopupViewSuggestionsDelegate;
@protocol OmniboxPopupPositioner;
struct AutocompleteMatch;
@class OmniboxPopupPresenter;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// iOS implementation of AutocompletePopupView.
class OmniboxPopupViewIOS : public OmniboxPopupView,
                            public OmniboxPopupMediatorDelegate {
 public:
  OmniboxPopupViewIOS(ios::ChromeBrowserState* browser_state,
                      OmniboxEditModel* edit_model,
                      OmniboxPopupViewSuggestionsDelegate* delegate,
                      id<OmniboxPopupPositioner> positioner);
  ~OmniboxPopupViewIOS() override;

  // AutocompletePopupView implementation.
  bool IsOpen() const override;
  void InvalidateLine(size_t line) override {}
  void OnLineSelected(size_t line) override {}
  void UpdatePopupAppearance() override;
  void OnMatchIconUpdated(size_t match_index) override {}
  gfx::Rect GetTargetBounds() override;
  void PaintUpdatesNow() override {}
  void OnDragCanceled() override {}

  void UpdateEditViewIcon();
  void SetTextAlignment(NSTextAlignment alignment);

  // OmniboxPopupViewControllerDelegate implementation.
  bool IsStarredMatch(const AutocompleteMatch& match) const override;
  void OnMatchSelected(const AutocompleteMatch& match, size_t row) override;
  void OnMatchSelectedForAppending(const AutocompleteMatch& match) override;
  void OnMatchSelectedForDeletion(const AutocompleteMatch& match) override;
  void OnScroll() override;

 private:
  std::unique_ptr<OmniboxPopupModel> model_;
  OmniboxPopupViewSuggestionsDelegate* delegate_;  // weak
  base::scoped_nsobject<OmniboxPopupMediator> mediator_;
  base::scoped_nsobject<OmniboxPopupPresenter> presenter_;
  base::scoped_nsobject<OmniboxPopupViewController> popup_controller_;
  bool is_open_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_IOS_H_

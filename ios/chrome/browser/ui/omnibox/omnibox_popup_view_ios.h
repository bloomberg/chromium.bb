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

class OmniboxEditModel;
@class OmniboxPopupMaterialViewController;
class OmniboxPopupModel;
class OmniboxPopupViewSuggestionsDelegate;
@protocol OmniboxPopupPositioner;
struct AutocompleteMatch;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// iOS implementation of AutocompletePopupView.
class OmniboxPopupViewIOS : public OmniboxPopupView {
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
  gfx::Rect GetTargetBounds() override;
  void PaintUpdatesNow() override {}
  void OnDragCanceled() override {}

  void OpenURLForRow(size_t row);
  void DidScroll();
  void UpdateEditViewIcon();
  void CopyToOmnibox(const base::string16& text);
  void SetTextAlignment(NSTextAlignment alignment);
  bool IsStarredMatch(const AutocompleteMatch& match) const;
  void DeleteMatch(const AutocompleteMatch& match) const;

 private:
  std::unique_ptr<OmniboxPopupModel> model_;
  OmniboxPopupViewSuggestionsDelegate* delegate_;  // weak
  __weak id<OmniboxPopupPositioner> positioner_;
  // View that contains the omnibox popup table view and shadow.
  base::scoped_nsobject<UIView> popupView_;
  base::scoped_nsobject<OmniboxPopupMaterialViewController> popup_controller_;
  bool is_open_;
  // Animate the appearance of the omnibox popup view.
  void AnimateDropdownExpansion(CGFloat parentHeight);
  // Animate the disappearance of the omnibox popup view.
  void AnimateDropdownCollapse();
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_IOS_H_

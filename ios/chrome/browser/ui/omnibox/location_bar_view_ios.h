// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_controller.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

class OmniboxViewIOS;
@class OmniboxClearButtonBridge;
@protocol OmniboxPopupPositioner;
@class OmniboxTextFieldIOS;
@protocol PreloadProvider;
class ToolbarModel;

// Delegate for LocationBarViewIOS objects.  Used to provide the location bar a
// way to open URLs and otherwise interact with the browser.
@protocol LocationBarDelegate
- (void)loadGURLFromLocationBar:(const GURL&)url
                     transition:(ui::PageTransition)transition;
- (void)locationBarHasBecomeFirstResponder;
- (void)locationBarHasResignedFirstResponder;
- (void)locationBarBeganEdit;
- (void)locationBarChanged;
- (web::WebState*)getWebState;
- (ToolbarModel*)toolbarModel;
@end

// C++ object that wraps an OmniboxViewIOS and serves as its
// OmniboxEditController.  LocationBarViewIOS bridges between the edit view
// and the rest of the browser and manages text field decorations (location
// icon, security icon, etc.).
class LocationBarViewIOS : public WebOmniboxEditController {
 public:
  LocationBarViewIOS(OmniboxTextFieldIOS* field,
                     ios::ChromeBrowserState* browser_state,
                     id<PreloadProvider> preloader,
                     id<OmniboxPopupPositioner> positioner,
                     id<LocationBarDelegate> delegate);
  ~LocationBarViewIOS() override;

  // OmniboxEditController implementation
  void OnAutocompleteAccept(const GURL& url,
                            WindowOpenDisposition disposition,
                            ui::PageTransition transition,
                            AutocompleteMatchType::Type type) override;
  void OnChanged() override;
  void OnInputInProgress(bool in_progress) override;
  void OnSetFocus() override;
  ToolbarModel* GetToolbarModel() override;
  const ToolbarModel* GetToolbarModel() const override;

  // WebOmniboxEditController implementation.
  web::WebState* GetWebState() override;
  void OnKillFocus() override;

  // Called when toolbar state is updated.
  void OnToolbarUpdated();

  // Resign omnibox first responder and end edit view editing.
  void HideKeyboardAndEndEditing();

  // Tells the omnibox if it can show the hint text or not.
  void SetShouldShowHintText(bool show_hint_text);

  // Returns a pointer to the text entry view.
  const OmniboxView* GetLocationEntry() const;
  OmniboxView* GetLocationEntry();

  // True if the omnibox text field is showing a placeholder image in its left
  // view while it's collapsed (i.e. not in editing mode).
  bool IsShowingPlaceholderWhileCollapsed();

 private:
  // Installs a UIButton that serves as the location icon and lock icon.  This
  // button is installed as a left view of |field_|.
  void InstallLocationIcon();

  // Creates and installs the voice search UIButton as a right view of |field_|.
  // Does nothing on tablet.
  void InstallVoiceSearchIcon();

  // Creates the clear text UIButton to be used as a right view of |field_|.
  void CreateClearTextIcon(bool is_incognito);

  // Updates the view to show the appropriate button (e.g. clear text or voice
  // search) on the right side of |field_|.
  void UpdateRightDecorations();

  bool show_hint_text_;
  base::scoped_nsobject<UIButton> clear_text_button_;
  std::unique_ptr<OmniboxViewIOS> edit_view_;
  base::scoped_nsobject<OmniboxClearButtonBridge> clear_button_bridge_;
  OmniboxTextFieldIOS* field_;
  id<LocationBarDelegate> delegate_;
  bool is_showing_placeholder_while_collapsed_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_VIEW_IOS_H_

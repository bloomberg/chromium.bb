// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_CONTROLLER_IMPL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_CONTROLLER_IMPL_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ios/shared/chrome/browser/ui/omnibox/location_bar_controller.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

@protocol LocationBarDelegate;
class OmniboxViewIOS;
@class OmniboxClearButtonBridge;
@protocol OmniboxPopupPositioner;
@class OmniboxTextFieldIOS;
@protocol PreloadProvider;
class ToolbarModel;

// Concrete implementation of the LocationBarController interface.
class LocationBarControllerImpl : public LocationBarController {
 public:
  LocationBarControllerImpl(OmniboxTextFieldIOS* field,
                            ios::ChromeBrowserState* browser_state,
                            id<PreloadProvider> preloader,
                            id<OmniboxPopupPositioner> positioner,
                            id<LocationBarDelegate> delegate);
  ~LocationBarControllerImpl() override;

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

  // LocationBarController implementation.
  void OnToolbarUpdated() override;
  void HideKeyboardAndEndEditing() override;
  void SetShouldShowHintText(bool show_hint_text) override;
  const OmniboxView* GetLocationEntry() const override;
  OmniboxView* GetLocationEntry() override;
  bool IsShowingPlaceholderWhileCollapsed() override;

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
  // |field_| should be __weak but is included from non-ARC code.
  __unsafe_unretained OmniboxTextFieldIOS* field_;
  // |delegate_| should be __weak but is included from non-ARC code.
  __unsafe_unretained id<LocationBarDelegate> delegate_;
  bool is_showing_placeholder_while_collapsed_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_CONTROLLER_IMPL_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#import "chrome/browser/ui/cocoa/omnibox_decoration_bubble_controller.h"
#include "components/translate/core/common/translate_errors.h"

@class BrowserWindowController;

class LanguageComboboxModel;
class TranslateBubbleModel;
class TranslateDenialComboboxModel;

namespace content {
class WebContents;
}

// Displays the Translate bubble. The Translate bubble is a bubble which
// pops up when clicking the Translate icon on Omnibox. This bubble
// allows us to translate a foreign page into user-selected language,
// revert this, and configure the translate setting.
@interface TranslateBubbleController
    : OmniboxDecorationBubbleController<NSTextViewDelegate> {
 @private
  content::WebContents* webContents_;
  std::unique_ptr<TranslateBubbleModel> model_;

  // The views of each state. The keys are TranslateBubbleModel::ViewState,
  // and the values are NSView*.
  base::scoped_nsobject<NSDictionary> views_;

  // The 'Done' or 'Translate' button on the advanced (option) panel.
  NSButton* advancedDoneButton_;

  // The 'Cancel' button on the advanced (option) panel.
  NSButton* advancedCancelButton_;

  // The 'Always translate' checkbox on the before panel.
  // This is nil when the current WebContents is in an incognito window.
  NSButton* beforeAlwaysTranslateCheckbox_;

  // The 'Always translate' checkbox on the advanced (option) panel.
  // This is nil when the current WebContents is in an incognito window.
  NSButton* advancedAlwaysTranslateCheckbox_;

  // The 'Try again' button on the error panel.
  NSButton* tryAgainButton_;

  // The '[x]' close button on the upper right side of the before panel.
  NSButton* closeButton_;

  // The combobox model which is used to deny translation at the view before
  // translate.
  std::unique_ptr<TranslateDenialComboboxModel> translateDenialComboboxModel_;

  // The combobox model for source languages on the advanced (option) panel.
  std::unique_ptr<LanguageComboboxModel> sourceLanguageComboboxModel_;

  // The combobox model for target languages on the advanced (option) panel.
  std::unique_ptr<LanguageComboboxModel> targetLanguageComboboxModel_;

  // Whether the translation is actually executed once at least.
  BOOL translateExecuted_;

  // The state of the 'Always ...' checkboxes.
  BOOL shouldAlwaysTranslate_;
}

@property(readonly, nonatomic) const content::WebContents* webContents;
@property(readonly, nonatomic) const TranslateBubbleModel* model;

- (id)initWithParentWindow:(BrowserWindowController*)controller
                     model:(std::unique_ptr<TranslateBubbleModel>)model
               webContents:(content::WebContents*)webContents;
- (void)switchView:(TranslateBubbleModel::ViewState)viewState;
- (void)switchToErrorView:(translate::TranslateErrors::Type)errorType;

@end

// The methods on this category are used internally by the controller and are
// only exposed for testing purposes. DO NOT USE OTHERWISE.
@interface TranslateBubbleController (ExposedForTesting)
- (IBAction)handleCloseButtonPressed:(id)sender;
- (IBAction)handleTranslateButtonPressed:(id)sender;
@end

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
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
@interface TranslateBubbleController : BaseBubbleController {
 @private
  content::WebContents* webContents_;
  scoped_ptr<TranslateBubbleModel> model_;

  // The views of each state. The keys are TranslateBubbleModel::ViewState,
  // and the values are NSView*.
  base::scoped_nsobject<NSDictionary> views_;

  // The 'Done' or 'Translate' button on the advanced (option) panel.
  NSButton* advancedDoneButton_;

  // The 'Cancel' button on the advanced (option) panel.
  NSButton* advancedCancelButton_;

  // The 'Always translate' checkbox on the advanced (option) panel.
  // This is nil when the current WebContents is in an incognito window.
  NSButton* alwaysTranslateCheckbox_;

  // The combobox model which is used to deny translation at the view before
  // translate.
  scoped_ptr<TranslateDenialComboboxModel> translateDenialComboboxModel_;

  // The combobox model for source languages on the advanced (option) panel.
  scoped_ptr<LanguageComboboxModel> sourceLanguageComboboxModel_;

  // The combobox model for target languages on the advanced (option) panel.
  scoped_ptr<LanguageComboboxModel> targetLanguageComboboxModel_;

  // Whether the translation is actually executed once at least.
  BOOL translateExecuted_;
}

@property(readonly, nonatomic) const content::WebContents* webContents;
@property(readonly, nonatomic) const TranslateBubbleModel* model;

- (id)initWithParentWindow:(BrowserWindowController*)controller
                     model:(scoped_ptr<TranslateBubbleModel>)model
               webContents:(content::WebContents*)webContents;
- (void)switchView:(TranslateBubbleModel::ViewState)viewState;
- (void)switchToErrorView:(translate::TranslateErrors::Type)errorType;

@end

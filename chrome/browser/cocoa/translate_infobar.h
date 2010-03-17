// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/infobar_controller.h"

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/translate/languages_menu_model.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/common/notification_registrar.h"

class TranslateInfoBarMenuModel;
class TranslateNotificationObserverBridge;

// Draws and maintains Translate Infobar GUI.
// The translate bar can be in one of 3 states:
// 1. "Before Translate" - source language popup and translate/cancel buttons
//    visible.
// 2. "Translating" - "Translating..." status text visible in address bar.
// 3. "After Translation" - source & target language popups visible.
//
// The following state transitions are supported:
// 1->{2,3}
// 2<->3
// i.e. Once you've transitioned out of "Before Translate" mode you can't switch
// back, however all other state transitions are supported.
//
// The GUI uses popup menus interspersed in a text label.  For localization
// purposes this means we potentially need 3 labels to display the UI (the 3rd
// is only visible in certain locales).
@interface TranslateInfoBarController : InfoBarController {
 @protected
  // label_ is defined in InfoBarController.
  scoped_nsobject<NSTextField> label2_;
  scoped_nsobject<NSTextField> label3_;
  scoped_nsobject<NSTextField> translatingLabel_;
  scoped_nsobject<NSPopUpButton> fromLanguagePopUp_;
  scoped_nsobject<NSPopUpButton> toLanguagePopUp_;
  scoped_nsobject<NSPopUpButton> optionsPopUp_;

  // In the current locale, are the "from" and "to" language popup menu
  // flipped from what they'd appear in English.
  bool swappedLanguagePlaceholders_;

  // Space between controls in pixels - read from the NIB.
  CGFloat spaceBetweenControls_;
  int numLabelsDisplayed_;

  scoped_ptr<LanguagesMenuModel> original_language_menu_model_;
  scoped_ptr<LanguagesMenuModel> target_language_menu_model_;
  scoped_ptr<OptionsMenuModel> options_menu_model_;
  scoped_ptr<TranslateInfoBarMenuModel> menu_model_;
  scoped_ptr<TranslateNotificationObserverBridge> observer_bridge_;
}

@end

@interface TranslateInfoBarController (TestingAPI)

// Main function to update the toolbar graphic state and data model after
// the state has changed.
// Controls are moved around as needed and visibility changed to match the
// current state.
- (void)updateState;

// Called when the source or target language selection changes in a menu.
// |newLanguageIdx| is the index of the newly selected item in the appropriate
// menu.
- (void)sourceLanguageModified:(NSInteger)newLanguageIdx;
- (void)targetLanguageModified:(NSInteger)newLanguageIdx;

// Called when an item in one of the toolbar's menus is selected.
- (void)menuItemSelected:(id)item;

// Returns the underlying options menu.
- (NSMenu*)optionsMenu;

// Verifies that the layout of the infobar is correct for |state|.
- (bool)verifyLayout:(TranslateInfoBarDelegate::TranslateState)state;

// Teardown and rebuild the options menu.
- (void)rebuildOptionsMenu;

@end

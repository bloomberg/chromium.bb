// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_OPTION_SELECTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_OPTION_SELECTION_DELEGATE_H_

#import <Foundation/Foundation.h>

@class PopupMenuPresenter;
@class PopupMenuTableViewController;

// Protocol adopted by the object receiving user choices of the translate
// options.
@protocol TranslateOptionSelectionDelegate

// Tells the delegate that the user chose to see the list of target languages.
- (void)popupMenuTableViewControllerDidSelectTargetLanguageSelector:
    (PopupMenuTableViewController*)sender;

// Tells the delegate that the user chose to always translate sites in the
// source language.
- (void)popupMenuTableViewControllerDidSelectAlwaysTranslateSourceLanguage:
    (PopupMenuTableViewController*)sender;

// Tells the delegate that the user chose to never translate sites in the source
// language.
- (void)popupMenuTableViewControllerDidSelectNeverTranslateSourceLanguage:
    (PopupMenuTableViewController*)sender;

// Tells the delegate that the user chose to never translate the current site.
- (void)popupMenuTableViewControllerDidSelectNeverTranslateSite:
    (PopupMenuTableViewController*)sender;

// Tells the delegate that the user chose to see the list of source languages.
- (void)popupMenuTableViewControllerDidSelectSourceLanguageSelector:
    (PopupMenuTableViewController*)sender;

// Tells the delegate that the user chose to close the menu without making a
// selection.
- (void)popupMenuPresenterDidCloseWithoutSelection:(PopupMenuPresenter*)sender;

@end

#endif  // IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_OPTION_SELECTION_DELEGATE_H_

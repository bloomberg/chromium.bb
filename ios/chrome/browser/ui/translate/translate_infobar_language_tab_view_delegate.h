// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

// A protocol implemented by a delegate of TranslateInfobarLanguageTabView.
@protocol TranslateInfobarLanguageTabViewDelegate

// Notifies the delegate that user tapped the view.
- (void)translateInfobarTabViewDidTap:(TranslateInfobarLanguageTabView*)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_VIEW_DELEGATE_H_

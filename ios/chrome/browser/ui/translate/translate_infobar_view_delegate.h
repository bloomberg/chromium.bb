// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

// A protocol implemented by a delegate of TranslateInfobarView.
@protocol TranslateInfobarViewDelegate

// Notifies the delegate that user tapped the source language button.
- (void)translateInfobarViewDidTapSourceLangugage:(TranslateInfobarView*)sender;

// Notifies the delegate that user tapped the target language button.
- (void)translateInfobarViewDidTapTargetLangugage:(TranslateInfobarView*)sender;

// Notifies the delegate that user tapped the options button.
- (void)translateInfobarViewDidTapOptions:(TranslateInfobarView*)sender;

// Notifies the delegate that user tapped the dismiss button.
- (void)translateInfobarViewDidTapDismiss:(TranslateInfobarView*)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_VIEW_DELEGATE_H_

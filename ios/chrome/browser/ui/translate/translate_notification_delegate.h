// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_DELEGATE_H_

@protocol TranslateNotificationHandler;

// Protocol adopted by an object that gets notified when the translate
// notification UI is dismissed, automatically or by the user.
@protocol TranslateNotificationDelegate

// Tells the delegate that the notification to inform the user that the source
// language will always be translated was dismissed.
- (void)translateNotificationHandlerDidDismissAlwaysTranslateLanguage:
    (id<TranslateNotificationHandler>)sender;

// Tells the delegate that the notification to inform the user that the source
// language will never be translated was dismissed.
- (void)translateNotificationHandlerDidDismissNeverTranslateLanguage:
    (id<TranslateNotificationHandler>)sender;

// Tells the delegate that the notification to inform the user that the site
// will never be translated was dismissed.
- (void)translateNotificationHandlerDidDismissNeverTranslateSite:
    (id<TranslateNotificationHandler>)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_DELEGATE_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_HANDLER_H_

@protocol TranslateNotificationDelegate;

// Protocol adopted by an object that displays translate notifications.
@protocol TranslateNotificationHandler

// Tells the handler to display a notification that |sourceLanguage| will always
// be translated to |targetLanguage| and inform |delegate| of its dismissal.
- (void)showAlwaysTranslateLanguageNotificationWithDelegate:
            (id<TranslateNotificationDelegate>)delegate
                                             sourceLanguage:
                                                 (NSString*)sourceLanguage
                                             targetLanguage:
                                                 (NSString*)targetLanguage;

// Tells the handler to display a notification that |sourceLanguage| will never
// be translated and inform |delegate| of its dismissal.
- (void)showNeverTranslateLanguageNotificationWithDelegate:
            (id<TranslateNotificationDelegate>)delegate
                                            sourceLanguage:
                                                (NSString*)sourceLanguage;

// Tells the handler to display a notification that this site will never be
// translated and inform |delegate| of its dismissal.
- (void)showNeverTranslateSiteNotificationWithDelegate:
    (id<TranslateNotificationDelegate>)delegate;

// Tells the handler to stop displaying the notification, if any.
- (void)dismissNotification;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_HANDLER_H_

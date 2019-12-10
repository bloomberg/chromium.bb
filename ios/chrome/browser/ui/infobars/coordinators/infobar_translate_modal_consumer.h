// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_

namespace {
// Pref keys passed through setupModalViewControllerWithPrefs:.
NSString* kSourceLanguagePrefKey = @"sourceLanguage";
NSString* kTargetLanguagePrefKey = @"targetLanguage";
NSString* kEnableTranslateButtonPrefKey = @"enableTranslateButton";
NSString* kUpdateLanguageBeforeTranslatePrefKey =
    @"updateLanguageBeforeTranslate";
NSString* kEnableAndDisplayShowOriginalButtonPrefKey =
    @"enableAndDisplayShowOriginalButton";
NSString* kShouldAlwaysTranslatePrefKey = @"shouldAlwaysTranslate";
NSString* kDisplayNeverTranslateLanguagePrefKey =
    @"displayNeverTranslateLanguage";
NSString* kIsTranslatableLanguagePrefKey = @"isTranslatableLanguage";
NSString* kDisplayNeverTranslateSiteButtonPrefKey =
    @"displayNeverTranslateSite";
NSString* kIsSiteBlacklistedPrefKey = @"isSiteBlacklisted";

}

// Protocol consumer used to push information to the Infobar Translate Modal UI
// for it to properly configure itself.
@protocol InfobarTranslateModalConsumer <NSObject>

// Informs the consumer of the current state of important prefs.
- (void)setupModalViewControllerWithPrefs:
    (NSDictionary<NSString*, NSObject*>*)prefs;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_MODAL_CONSUMER_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_BEFORE_TRANSLATE_INFOBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TRANSLATE_BEFORE_TRANSLATE_INFOBAR_CONTROLLER_H_

#include "ios/chrome/browser/infobars/infobar_controller.h"

// The accessibility identifier of the cancel button on language picker view.
extern NSString* const kLanguagePickerCancelButtonId;

// The accessibility identifier of the done button on language picker view.
extern NSString* const kLanguagePickerDoneButtonId;

@interface BeforeTranslateInfoBarController : InfoBarController

@end

#endif  // IOS_CHROME_BROWSER_TRANSLATE_BEFORE_TRANSLATE_INFOBAR_CONTROLLER_H_

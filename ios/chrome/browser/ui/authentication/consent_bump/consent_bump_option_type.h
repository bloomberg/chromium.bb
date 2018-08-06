// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_OPTION_TYPE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_OPTION_TYPE_H_

#import <Foundation/Foundation.h>

// Different option types.
typedef NS_ENUM(NSInteger, ConsentBumpOptionType) {
  ConsentBumpOptionTypeNotSet = 0,
  ConsentBumpOptionTypeNoChange,
  ConsentBumpOptionTypeReview,
  ConsentBumpOptionTypeTurnOn
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_OPTION_TYPE_H_

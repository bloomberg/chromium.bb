// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_PREFERENCES_AUTOFILL_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_PREFERENCES_AUTOFILL_H_

#import <Foundation/Foundation.h>

#import "cwv_preferences.h"

@interface CWVPreferences (Autofill)

// Whether or not profile autofill is turned on. Defaults to |YES|.
// If enabled, contents of submitted profiles may be saved and offered as a
// suggestion in either the same or similar forms.
@property(nonatomic, assign, getter=isProfileAutofillEnabled)
    BOOL profileAutofillEnabled;

// Whether or not credit card autofill is turned on. Defaults to |YES|.
// If enabled, contents of submitted credit cards may be saved and offered as a
// suggestion in either the same or similar forms.
@property(nonatomic, assign, getter=isCreditCardAutofillEnabled)
    BOOL creditCardAutofillEnabled;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_PREFERENCES_AUTOFILL_H_

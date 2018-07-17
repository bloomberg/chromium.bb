// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_ACCESSORY_PROVIDER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_ACCESSORY_PROVIDER_H_

#import "ios/chrome/browser/autofill/form_input_accessory_view_provider.h"

// Returns a default keyboard accessory view with entry points to manual
// fallbacks for form filling.
@interface ManualfillAccessoryProvider
    : NSObject<FormInputAccessoryViewProvider>

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_ACCESSORY_PROVIDER_H_

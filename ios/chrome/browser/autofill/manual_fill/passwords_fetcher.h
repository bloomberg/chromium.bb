// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PASSWORDS_FETCHER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PASSWORDS_FETCHER_H_

#import <Foundation/Foundation.h>
#include <memory>
#include <vector>

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@class PasswordFetcher;

// Protocol to receive the passwords fetched asynchronously.
@protocol PasswordFetcherDelegate

// Saved passwords has been fetched or updated.
- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<autofill::PasswordForm>>&)passwords;

@end

@interface PasswordFetcher : NSObject

// The designated initializer. |browserState| must not be nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:(id<PasswordFetcherDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PASSWORDS_FETCHER_H_

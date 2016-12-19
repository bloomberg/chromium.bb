// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_DIALOG_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_DIALOG_DELEGATE_H_

@class Tab;

// A protocol delegating the display of dialogs for a Tab.
@protocol TabDialogDelegate

// Displays an HTTP authentication dialog. |completionHandler| should be called
// with non nil |username| and |password| in order to proceed with
// authentication.
- (void)tab:(Tab*)tab
    runAuthDialogForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                 proposedCredential:(NSURLCredential*)credential
                  completionHandler:
                      (void (^)(NSString* user, NSString* password))handler;

// Called by Tabs to cancel a previously-requested dialog.
- (void)cancelDialogForTab:(Tab*)tab;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_DIALOG_DELEGATE_H_

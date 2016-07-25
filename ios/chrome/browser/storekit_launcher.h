// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_STOREKIT_LAUNCHER_H_
#define IOS_CHROME_BROWSER_STOREKIT_LAUNCHER_H_

@class NSString;

// Protocol to be implemented by a class that provides an access to the app
// store with StoreKit.
@protocol StoreKitLauncher

// Opens StoreKit modal to present a product identified with |productID|.
- (void)openAppStore:(NSString*)productID;

@end

#endif  // IOS_CHROME_BROWSER_STOREKIT_LAUNCHER_H_

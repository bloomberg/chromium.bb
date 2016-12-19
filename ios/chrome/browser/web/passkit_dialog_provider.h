// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PASSKIT_DIALOG_PROVIDER_H_
#define IOS_CHROME_BROWSER_WEB_PASSKIT_DIALOG_PROVIDER_H_

// Protocol to be implemented by a class that provides a dialog to download
// a PassKit object.
@protocol PassKitDialogProvider

- (void)presentPassKitDialog:(NSData*)data;

@end

#endif  // IOS_CHROME_BROWSER_WEB_PASSKIT_DIALOG_PROVIDER_H_

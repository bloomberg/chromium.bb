// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_LEGACY_MAILTO_URL_REWRITER_H_
#define IOS_CHROME_BROWSER_WEB_LEGACY_MAILTO_URL_REWRITER_H_

#import "ios/chrome/browser/web/mailto_url_rewriter.h"

// An object that manages the available Mail client apps. The currently selected
// Mail client to handle mailto: URL is stored in a key in NSUserDefaults.
// If the key in NSUserDefaults is not set or the corresponding app is no longer
// installed, the system-provided Mail client app will be used.
@interface LegacyMailtoURLRewriter : MailtoURLRewriter

@end

#endif  // IOS_CHROME_BROWSER_WEB_LEGACY_MAILTO_URL_REWRITER_H_

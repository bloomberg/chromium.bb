// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_NULLABLE_MAILTO_URL_REWRITER_H_
#define IOS_CHROME_BROWSER_WEB_NULLABLE_MAILTO_URL_REWRITER_H_

#import "ios/chrome/browser/web/mailto_url_rewriter.h"

// An object that manages the available Mail client apps. The currently selected
// Mail client to handle mailto: URL is stored in a key in NSUserDefaults. If a
// default has not been set in NSUserDefaults, nil may be returned from some of
// the public APIs of MailtoURLRewriter.
@interface NullableMailtoURLRewriter : MailtoURLRewriter

@end

#endif  // IOS_CHROME_BROWSER_WEB_NULLABLE_MAILTO_URL_REWRITER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_VIEW_PUBLIC_CWV_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_DELEGATE_H_

// TODO(crbug.com/704946): Make framework style include work everywhere and
// remove this #if.
#if defined(CWV_IMPLEMENTATION)
#include "ios/web_view/public/cwv_export.h"
#else
#include <ChromeWebView/cwv_export.h>
#endif

@class NSString;

// Delegate interface for the CWV library.  Embedders can implement the
// functions in order to customize library behavior.
CWV_EXPORT
@protocol CWVDelegate

// Returns a partial user agent of the form "ProductName/Version".
- (NSString*)partialUserAgent;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_DELEGATE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_VIEW_PUBLIC_CRIWV_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CRIWV_DELEGATE_H_

@class NSString;

// Delegate interface for the CRIWV library.  Embedders can implement the
// functions in order to customize library behavior.
@protocol CRIWVDelegate

// Returns a partial user agent of the form "ProductName/Version".
- (NSString*)partialUserAgent;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CRIWV_DELEGATE_H_

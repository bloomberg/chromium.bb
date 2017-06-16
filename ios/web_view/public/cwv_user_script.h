// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_USER_SCRIPT_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_USER_SCRIPT_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

// User Script to be injected into main frame of CWVWebView after
// window.document is created, but before other content is loaded (i.e., at the
// same timing as WKUserScriptInjectionTimeAtDocumentStart).
CWV_EXPORT
@interface CWVUserScript : NSObject

// JavaScript source code.
@property(nonatomic, copy, readonly, nonnull) NSString* source;

- (nonnull instancetype)init NS_UNAVAILABLE;

- (nonnull instancetype)initWithSource:(nonnull NSString*)source;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_USER_SCRIPT_H_

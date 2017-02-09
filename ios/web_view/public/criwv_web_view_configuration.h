// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_CONFIGURATION_H_
#define IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@class CRIWVWebsiteDataStore;

// Configuration used for creation of a CRIWVWebView.
@interface CRIWVWebViewConfiguration : NSObject<NSCopying>

// Data store defining persistance of website data. Default is
// [CRIWVWebsiteDataStore defaultDataStore].
@property(nonatomic, strong, nonnull) CRIWVWebsiteDataStore* websiteDataStore;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_CONFIGURATION_H_

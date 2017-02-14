// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@class CWVWebsiteDataStore;

// Configuration used for creation of a CWVWebView.
@interface CWVWebViewConfiguration : NSObject<NSCopying>

// Data store defining persistance of website data. Default is
// [CWVWebsiteDataStore defaultDataStore].
@property(nonatomic, strong, nonnull) CWVWebsiteDataStore* websiteDataStore;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_H_

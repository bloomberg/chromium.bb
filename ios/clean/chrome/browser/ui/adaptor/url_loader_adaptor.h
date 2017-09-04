// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ADAPTOR_URL_LOADER_ADAPTOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ADAPTOR_URL_LOADER_ADAPTOR_H_

#import "ios/chrome/browser/ui/url_loader.h"

// URL loader to be used for components from the old architecture.
// TODO(crbug.com/740793): This class should be hooked with the new architecture
// way of loading URL. Once it is done, remove alerts
@interface URLLoaderAdaptor : NSObject<UrlLoader>

// TODO(crbug.com/740793): Remove alert once this class is functional.
@property(nonatomic, strong) UIViewController* viewControllerForAlert;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ADAPTOR_URL_LOADER_ADAPTOR_H_

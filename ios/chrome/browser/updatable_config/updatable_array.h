// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_ARRAY_H_
#define IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_ARRAY_H_

#import "ios/chrome/browser/updatable_config/updatable_config_base.h"
#import "ios/public/provider/chrome/browser/updatable_resource_provider.h"

// UpdatableResourceBridge supports data files of json or plist format but only
// allows dictionary types in the data. UpdatableArrayDelegate overrides the
// default file loader to accept plist file type with <array> data.
// This class is publicly declared here for unit tests to mock for testing
// and is not intended to be used elsewhere.
@interface UpdatableArrayDelegate : NSObject<UpdatableResourceDelegate>

// The array object loaded from updatable resource.
@property(nonatomic, readonly) NSArray* resourceArray;

@end

@interface UpdatableArray : UpdatableConfigBase

// Returns an autoreleased copy of the configuration array read from the
// file specified in the initializer. This implementation supports <array>
// data type specified as a plist only.
- (NSArray*)arrayFromConfig;

@end

#endif  // IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_ARRAY_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_DICTIONARY_H_
#define IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_DICTIONARY_H_

#import "ios/chrome/browser/updatable_config/updatable_config_base.h"

@interface UpdatableDictionary : UpdatableConfigBase

// Returns an autoreleased copy of the configuration dictionary read from
// the file specified in the initializer. Configuration content must be
// of dictionary type.
- (NSDictionary*)dictionaryFromConfig;

@end

#endif  // IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_DICTIONARY_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/updatable_config/updatable_dictionary.h"

#import "base/logging.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/updatable_resource_provider.h"

@implementation UpdatableDictionary

- (id<UpdatableResourceBridge>)newResource:(NSString*)resourceName {
  return ios::GetChromeBrowserProvider()
      ->GetUpdatableResourceProvider()
      ->CreateUpdatableResource(resourceName, nil);
}

- (NSDictionary*)dictionaryFromConfig {
  id configData = [[[self.updatableResource resourceData] retain] autorelease];
  DCHECK([configData isKindOfClass:[NSDictionary class]]);
  return configData;
}

@end

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/updatable_config/updatable_array.h"

#import "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/updatable_resource_provider.h"

@interface UpdatableArrayDelegate ()
// Private helper function to check that |path| is of plist type
// and read the content of |path| into |resourceArray_|.
- (void)setResourceArrayWithContentsOfFile:(NSString*)path;
@end

@implementation UpdatableArrayDelegate {
  base::scoped_nsobject<NSArray> _resourceArray;
}

- (void)loadDefaults:(id<UpdatableResourceBridge>)resource {
  NSString* path = resource.descriptor.bundleResourcePath;
  [self setResourceArrayWithContentsOfFile:path];
}

- (void)mergeUpdate:(id<UpdatableResourceBridge>)resource {
  NSString* path = resource.descriptor.resourcePath;
  if ([path isEqualToString:resource.descriptor.bundleResourcePath]) {
    // There's no need to merge, because the only resource present is the one
    // bundled with the app, and that was loaded by loadDefaults.
    return;
  }
  [self setResourceArrayWithContentsOfFile:path];
}

- (NSDictionary*)parseFileAt:(NSString*)path {
  // Overrides this method with NOTREACHED() because default implementation in
  // UpdatableResourceBridge is for NSDictionary only and results in opaque
  // errors when the data file is of <array> type.
  NOTREACHED();
  return nil;
}

- (NSArray*)resourceArray {
  return _resourceArray.get();
}

- (void)setResourceArrayWithContentsOfFile:(NSString*)path {
  NSString* extension = [[path pathExtension] lowercaseString];
  // Only plist file type is supported.
  DCHECK([extension isEqualToString:@"plist"]);
  _resourceArray.reset([[NSArray arrayWithContentsOfFile:path] retain]);
}

@end

@implementation UpdatableArray

- (id<UpdatableResourceBridge>)newResource:(NSString*)resourceName {
  base::scoped_nsobject<UpdatableArrayDelegate> delegate(
      [[UpdatableArrayDelegate alloc] init]);

  return ios::GetChromeBrowserProvider()
      ->GetUpdatableResourceProvider()
      ->CreateUpdatableResource(resourceName, delegate);
}

- (NSArray*)arrayFromConfig {
  id delegate = [[self updatableResource] delegate];
  DCHECK(delegate);
  DCHECK([delegate respondsToSelector:@selector(resourceArray)]);
  id configData = [[[delegate resourceArray] retain] autorelease];
  DCHECK([configData isKindOfClass:[NSArray class]]);
  return configData;
}

@end

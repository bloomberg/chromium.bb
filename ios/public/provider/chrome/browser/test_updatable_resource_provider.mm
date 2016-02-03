// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/test_updatable_resource_provider.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

#pragma mark - TestUpdatabeResourceDescriptor

// Dummy UpdatableResourceDescriptorBridge implementation.
@interface TestUpdatabeResourceDescriptor
    : NSObject<UpdatableResourceDescriptorBridge>
@property(nonatomic, readonly) NSURL* updateURL;
@property(nonatomic, copy) NSString* applicationIdentifier;
@property(nonatomic, copy) NSString* applicationVersion;
@property(nonatomic, copy) NSString* bundleResourcePath;
@property(nonatomic, copy) NSString* updateResourcePath;
@end

@implementation TestUpdatabeResourceDescriptor
@synthesize updateURL;
@synthesize applicationIdentifier;
@synthesize applicationVersion;
@synthesize bundleResourcePath;
@synthesize updateResourcePath;

- (void)updateCheckDidFinishWithSuccess:(BOOL)wasSuccessful {
}

- (NSString*)resourcePath {
  return nil;
}
@end

#pragma mark - TestUpdatableResource

// Dummy UpdatableResourceDescriptorBridge implementation that simply loads data
// from the specified plist file.
@interface TestUpdatableResource : NSObject<UpdatableResourceBridge>
@property(nonatomic, readonly) id<UpdatableResourceDescriptorBridge> descriptor;
- (instancetype)initWithDelegate:(id<UpdatableResourceDelegate>)delegate
                           plist:(NSString*)resource_identifier
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

@implementation TestUpdatableResource {
  base::scoped_nsprotocol<id<UpdatableResourceDelegate>> _delegate;
  base::scoped_nsprotocol<id<UpdatableResourceDescriptorBridge>> _descriptor;
}

- (instancetype)initWithDelegate:(id<UpdatableResourceDelegate>)delegate
                           plist:(NSString*)resourceIdentifier {
  if (self = [super init]) {
    _delegate.reset([delegate retain]);
    _descriptor.reset([[TestUpdatabeResourceDescriptor alloc] init]);
    DCHECK([resourceIdentifier hasSuffix:@".plist"])
        << "TestUpdatableResource supports only the plist format";
    [_descriptor setBundleResourcePath:resourceIdentifier];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (id<UpdatableResourceDescriptorBridge>)descriptor {
  return _descriptor.get();
}

- (id<UpdatableResourceDelegate>)delegate {
  return _delegate;
}

- (NSDictionary*)resourceData {
  return [NSDictionary
      dictionaryWithContentsOfFile:[_descriptor bundleResourcePath]];
}
- (void)loadDefaults {
  [_delegate loadDefaults:self];
}

@end

#pragma mark - TestUpdatableResourceProvider

namespace ios {

NSString* TestUpdatableResourceProvider::GetUpdateNotificationName() {
  return @"ResourceUpdatedTest";
}

id<UpdatableResourceBridge>
TestUpdatableResourceProvider::CreateUpdatableResource(
    NSString* resource_identifier,
    id<UpdatableResourceDelegate> delegate) {
  NSString* path =
      [NSString stringWithFormat:@"%@/gm-config/ANY/%@",
                                 [[NSBundle mainBundle] resourcePath],
                                 resource_identifier];
  return [[TestUpdatableResource alloc] initWithDelegate:delegate plist:path];
}

}  // namespace ios

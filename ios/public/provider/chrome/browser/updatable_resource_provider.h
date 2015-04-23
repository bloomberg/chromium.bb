// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UPDATABLE_RESOURCE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UPDATABLE_RESOURCE_PROVIDER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"

@protocol UpdatableResourceDelegate;
@protocol UpdatableResourceDescriptorBridge;

#pragma mark - UpdatableResourceBridge

// UpdatableResourceBridge represents an updatable resource.
@protocol UpdatableResourceBridge<NSObject>

// Returns the UpdatableResourceDescriptorBridge associated to this resource.
@property(nonatomic, readonly) id<UpdatableResourceDescriptorBridge> descriptor;

// Returns the UpdatableResourceDelegate associated to this resource.
- (id<UpdatableResourceDelegate>)delegate;

// The data provided by this resource.
- (NSDictionary*)resourceData;

// Initializes this updatable resource with default values.
- (void)loadDefaults;
@end

#pragma mark - UpdatableResourceDescriptorBridge

// This class encapsulates identification and versioning information
// for an updatable resource.
@protocol UpdatableResourceDescriptorBridge<NSObject>

@property(nonatomic, copy) NSString* applicationIdentifier;
@property(nonatomic, copy) NSString* applicationVersion;

// URL where an update to this resource may be downloaded.
@property(nonatomic, readonly) NSURL* updateURL;

// Search path for the default file.
@property(nonatomic, copy) NSString* bundleResourcePath;
// Search path for the downloaded file.
@property(nonatomic, copy) NSString* updateResourcePath;

// This method must be called after an update check has been made. Either
// successfully or not.
- (void)updateCheckDidFinishWithSuccess:(BOOL)wasSuccessful;

// Returns the full path of the latest and greatest version of the resource
// currently residing on the device. If there is no version of the resource
// currently stored on the device, returns |nil|.
- (NSString*)resourcePath;

@end

#pragma mark - UpdatableResourceDelegate

// Delegate for UpdatableResourceBridge.
@protocol UpdatableResourceDelegate<NSObject>

// This method can be reimplemented to override the behavior of the
// UpdatableResourceBridge method.
- (void)loadDefaults:(id<UpdatableResourceBridge>)resource;

// Merges the latest values defined in the file into the values currently stored
// by the updatable resource.
- (void)mergeUpdate:(id<UpdatableResourceBridge>)resource;

// Parses file at |path| and returns an NSDictionary with the values stored
// therein.
- (NSDictionary*)parseFileAt:(NSString*)path;
@end

#pragma mark - UpdatableResourceProvider

namespace ios {

// Provider for UpdatableResourceBridge.
class UpdatableResourceProvider {
 public:
  UpdatableResourceProvider();
  virtual ~UpdatableResourceProvider();

  // Returns the name of the notification that is sent through the notification
  // center when a resource is updated.
  virtual NSString* GetUpdateNotificationName();

  // Creates a new UpdatableResourceBrige.
  // The user is responsible for releasing it.
  // The UpdatableResourceBridge takes ownership of the delegate.
  // |delegate| may be nil.
  virtual id<UpdatableResourceBridge> CreateUpdatableResource(
      NSString* resource_identifier,
      id<UpdatableResourceDelegate> delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdatableResourceProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UPDATABLE_RESOURCE_PROVIDER_H_

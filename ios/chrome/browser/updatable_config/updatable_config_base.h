// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_CONFIG_BASE_H_
#define IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_CONFIG_BASE_H_

#import <Foundation/Foundation.h>

namespace net {
class URLRequestContextGetter;
}  // namespace net

@protocol UpdatableResourceBridge;

// Abstract base class for configurations bundled in a .plist file in
// application bundle. Configuration is periodically updated by pulling
// in a new configuration file from gstatic.com.
// DO NOT use this class directly, but instantiate either UpdatableArray
// or UpdatableDictionary class instead.
// |startUpdate:| must be called for every subclass.
@interface UpdatableConfigBase : NSObject

// Initializes a new Updatable Config object using .plist file named |plistName|
// which is in the application bundle. |appId| and |appVersion| are used to
// derive the URL to update this plist from gstatic.com.
// If |appId| is nil, a default value based on the application id from the
// application bundle will be provided.
// If |appVersion| is nil, a default value based on the application version
// number from the application bundle will be provided.
- (instancetype)initWithAppId:(NSString*)appId
                      version:(NSString*)appVersion
                        plist:(NSString*)plistName;

// Starts periodic checks with server for updated version of
// configuration plists
- (void)startUpdate:(net::URLRequestContextGetter*)requestContextGetter;

// Prevents any future update checks, and releases requestContextGetter. Should
// be called if the app is going to terminate.
- (void)stopUpdateChecks;

// Performs any post-update operations for the updated data to take effect.
- (void)resourceDidUpdate:(NSNotification*)notification;

// Subclasses must override this method to return a UpdatableResourceBridge
// object. The returned object determines whether the resource stored in the
// file |resourceName| is of plist, json, or some other data file format.
- (id<UpdatableResourceBridge>)newResource:(NSString*)resourceName;

// Returns the internal resources object. This accessor is intended for
// use by subclasses of this class and for unit testing only.
// Note that this is really a readonly property for subclasses, but for
// unit testing, the ability to set the resource is also needed.
@property(nonatomic, retain) id<UpdatableResourceBridge> updatableResource;

// Consistency check defaults to NO and is enabled only by Chrome application,
// not unit or kif tests.
+ (void)enableConsistencyCheck;

@end

#endif  // IOS_CHROME_BROWSER_UPDATABLE_CONFIG_UPDATABLE_CONFIG_BASE_H_

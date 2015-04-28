// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/test_location_manager.h"

#include "base/mac/scoped_nsobject.h"

@interface TestLocationManager () {
  CLAuthorizationStatus _authorizationStatus;
  base::scoped_nsobject<CLLocation> _currentLocation;
  BOOL _locationServicesEnabled;
  BOOL _started;
  BOOL _stopped;
}

@end

@implementation TestLocationManager

@synthesize authorizationStatus = _authorizationStatus;
@synthesize locationServicesEnabled = _locationServicesEnabled;
@synthesize started = _started;
@synthesize stopped = _stopped;

- (id)init {
  self = [super init];
  if (self) {
    [self reset];
  }
  return self;
}

- (void)setAuthorizationStatus:(CLAuthorizationStatus)authorizationStatus {
  if (_authorizationStatus != authorizationStatus) {
    _authorizationStatus = authorizationStatus;
    [self.delegate locationManagerDidChangeAuthorizationStatus:self];
  }
}

- (CLLocation*)currentLocation {
  return _currentLocation;
}

- (void)setCurrentLocation:(CLLocation*)currentLocation {
  _currentLocation.reset([currentLocation retain]);
}

- (void)reset {
  _authorizationStatus = kCLAuthorizationStatusNotDetermined;
  _currentLocation.reset();
  _locationServicesEnabled = YES;
  _started = NO;
  _stopped = NO;
}

#pragma mark - LocationManager overrides

- (void)startUpdatingLocation {
  _started = YES;
}

- (void)stopUpdatingLocation {
  _stopped = YES;
}

@end

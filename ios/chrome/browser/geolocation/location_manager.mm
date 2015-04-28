// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/location_manager.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/geolocation/CLLocation+OmniboxGeolocation.h"
#import "ios/chrome/browser/geolocation/location_manager+Testing.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/geolocation_updater_provider.h"

namespace {

const CLLocationDistance kLocationDesiredAccuracy =
    kCLLocationAccuracyHundredMeters;
// Number of seconds to wait before automatically stopping location updates.
const NSTimeInterval kLocationStopUpdateDelay = 5.0;
// A large value to disable automatic location updates in GeolocationUpdater.
const NSTimeInterval kLocationUpdateInterval = 365.0 * 24.0 * 60.0 * 60.0;

}  // namespace

@interface LocationManager () {
  base::scoped_nsprotocol<id<GeolocationUpdater>> _locationUpdater;
  base::scoped_nsobject<CLLocation> _currentLocation;
  base::WeakNSProtocol<id<LocationManagerDelegate>> _delegate;
  base::scoped_nsobject<NSDate> _startTime;
}

// Handles GeolocationUpdater notification for an updated device location.
- (void)handleLocationUpdateNotification:(NSNotification*)notification;
// Handles GeolocationUpdater notification for ending device location updates.
- (void)handleLocationStopNotification:(NSNotification*)notification;
// Handles GeolocationUpdater notification for changing authorization.
- (void)handleAuthorizationChangeNotification:(NSNotification*)notification;

@end

@implementation LocationManager

- (id)init {
  self = [super init];
  if (self) {
    ios::GeolocationUpdaterProvider* provider =
        ios::GetChromeBrowserProvider()->GetGeolocationUpdaterProvider();

    // |provider| may be null in tests.
    if (provider) {
      _locationUpdater.reset(provider->CreateGeolocationUpdater(false));
      [_locationUpdater setDesiredAccuracy:kLocationDesiredAccuracy
                            distanceFilter:kLocationDesiredAccuracy / 2];
      [_locationUpdater setStopUpdateDelay:kLocationStopUpdateDelay];
      [_locationUpdater setUpdateInterval:kLocationUpdateInterval];

      NSNotificationCenter* defaultCenter =
          [NSNotificationCenter defaultCenter];
      [defaultCenter addObserver:self
                        selector:@selector(handleLocationUpdateNotification:)
                            name:provider->GetUpdateNotificationName()
                          object:_locationUpdater];
      [defaultCenter addObserver:self
                        selector:@selector(handleLocationStopNotification:)
                            name:provider->GetStopNotificationName()
                          object:_locationUpdater];
      [defaultCenter
          addObserver:self
             selector:@selector(handleAuthorizationChangeNotification:)
                 name:provider->GetAuthorizationChangeNotificationName()
               object:nil];
    }
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (CLAuthorizationStatus)authorizationStatus {
  return [CLLocationManager authorizationStatus];
}

- (CLLocation*)currentLocation {
  if (!_currentLocation)
    _currentLocation.reset([[_locationUpdater currentLocation] retain]);
  return _currentLocation;
}

- (id<LocationManagerDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id<LocationManagerDelegate>)delegate {
  _delegate.reset(delegate);
}

- (BOOL)locationServicesEnabled {
  return [CLLocationManager locationServicesEnabled];
}

- (void)startUpdatingLocation {
  CLLocation* currentLocation = self.currentLocation;
  if (!currentLocation || [currentLocation cr_shouldRefresh]) {
    if (![_locationUpdater isEnabled])
      _startTime.reset([[NSDate alloc] init]);

    [_locationUpdater requestWhenInUseAuthorization];
    [_locationUpdater setEnabled:YES];
  }
}

- (void)stopUpdatingLocation {
  [_locationUpdater setEnabled:NO];
}

#pragma mark - Private

- (void)handleLocationUpdateNotification:(NSNotification*)notification {
  NSString* newLocationKey = ios::GetChromeBrowserProvider()
                                 ->GetGeolocationUpdaterProvider()
                                 ->GetUpdateNewLocationKey();
  CLLocation* location = [[notification userInfo] objectForKey:newLocationKey];
  if (location) {
    _currentLocation.reset([location retain]);

    if (_startTime) {
      NSTimeInterval interval = -[_startTime timeIntervalSinceNow];
      [_currentLocation cr_setAcquisitionInterval:interval];
    }
  }
}

- (void)handleLocationStopNotification:(NSNotification*)notification {
  [_locationUpdater setEnabled:NO];
}

- (void)handleAuthorizationChangeNotification:(NSNotification*)notification {
  [_delegate locationManagerDidChangeAuthorizationStatus:self];
}

#pragma mark - LocationManager+Testing

- (void)setGeolocationUpdater:(id<GeolocationUpdater>)geolocationUpdater {
  _locationUpdater.reset([geolocationUpdater retain]);
}

@end

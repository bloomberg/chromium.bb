// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the class definitions for the CoreLocation data provider
// class and the accompanying Objective C wrapper class. This data provider
// is used to allow the CoreLocation wrapper to run on the UI thread, since
// CLLocationManager's start and stop updating methods must be called from a
// thread with an active NSRunLoop.  Currently only the UI thread appears to
// fill that requirement.

#include "content/browser/geolocation/core_location_data_provider_mac.h"

#include "base/logging.h"
#include "base/time.h"
#include "content/browser/geolocation/core_location_provider_mac.h"
#include "content/browser/geolocation/geolocation_provider.h"

// A few required declarations since the CoreLocation headers are not available
// with the Mac OS X 10.5 SDK.
// TODO(jorgevillatoro): Remove these declarations when we build against 10.6

// This idea was borrowed from wifi_data_provider_corewlan_mac.mm
typedef double CLLocationDegrees;
typedef double CLLocationAccuracy;
typedef double CLLocationSpeed;
typedef double CLLocationDirection;
typedef double CLLocationDistance;
typedef struct {
  CLLocationDegrees latitude;
  CLLocationDegrees longitude;
} CLLocationCoordinate2D;

enum {
  kCLErrorLocationUnknown  = 0,
  kCLErrorDenied
};

@interface CLLocationManager : NSObject
+ (BOOL)locationServicesEnabled;
@property(assign) id delegate;
- (void)startUpdatingLocation;
- (void)stopUpdatingLocation;
@end

@interface CLLocation : NSObject<NSCopying, NSCoding>
@property(readonly) CLLocationCoordinate2D coordinate;
@property(readonly) CLLocationDistance altitude;
@property(readonly) CLLocationAccuracy horizontalAccuracy;
@property(readonly) CLLocationAccuracy verticalAccuracy;
@property(readonly) CLLocationDirection course;
@property(readonly) CLLocationSpeed speed;
@end

@protocol CLLocationManagerDelegate
- (void)locationManager:(CLLocationManager*)manager
    didUpdateToLocation:(CLLocation*)newLocation
           fromLocation:(CLLocation*)oldLocation;
- (void)locationManager:(CLLocationManager*)manager
       didFailWithError:(NSError*)error;
@end

// This wrapper class receives CLLocation objects from CoreLocation, converts
// them to Geoposition objects, and passes them on to the data provider class
// Note: This class has some specific threading requirements, inherited from
//       CLLocationManager.  The location manaager's start and stop updating
//       methods must be called from a thread that has an active run loop (which
//       seems to only be the UI thread)
@interface CoreLocationWrapperMac : NSObject<CLLocationManagerDelegate>
{
 @private
  NSBundle* bundle_;
  Class locationManagerClass_;
  id locationManager_;
  CoreLocationDataProviderMac* dataProvider_;
}

- (id)initWithDataProvider:(CoreLocationDataProviderMac*)dataProvider;
- (void)dealloc;

// Can be called from any thread since it does not require an NSRunLoop. However
// it is not threadsafe to receive concurrent calls until after it's first
// successful call (to avoid |bundle_| being double initialized)
- (BOOL)locationDataAvailable;

// These should always be called from BrowserThread::UI
- (void)startLocation;
- (void)stopLocation;

// These should only be called by CLLocationManager
- (void)locationManager:(CLLocationManager*)manager
    didUpdateToLocation:(CLLocation*)newLocation
           fromLocation:(CLLocation*)oldLocation;
- (void)locationManager:(CLLocationManager*)manager
       didFailWithError:(NSError*)error;
- (BOOL)loadCoreLocationBundle;

@end

@implementation CoreLocationWrapperMac

- (id)initWithDataProvider:(CoreLocationDataProviderMac*)dataProvider {
  DCHECK(dataProvider);
  dataProvider_ = dataProvider;
  self = [super init];
  return self;
}

- (void)dealloc {
  [locationManager_ release];
  [locationManagerClass_ release];
  [bundle_ release];
  [super dealloc];
}

// Load the bundle and check to see if location services are enabled
// but don't do anything else
- (BOOL)locationDataAvailable {
  return ([self loadCoreLocationBundle] &&
          [locationManagerClass_ locationServicesEnabled]);
}

- (void)startLocation {
  if ([self locationDataAvailable]) {
    if (!locationManager_) {
      locationManager_ = [[locationManagerClass_ alloc] init];
      [locationManager_ setDelegate:self];
    }
    [locationManager_ startUpdatingLocation];
  }
}

- (void)stopLocation {
  [locationManager_ stopUpdatingLocation];
}

- (void)locationManager:(CLLocationManager*)manager
    didUpdateToLocation:(CLLocation*)newLocation
           fromLocation:(CLLocation*)oldLocation {
  Geoposition position;
  position.latitude  = [newLocation coordinate].latitude;
  position.longitude = [newLocation coordinate].longitude;
  position.altitude  = [newLocation altitude];
  position.accuracy  = [newLocation horizontalAccuracy];
  position.altitude_accuracy = [newLocation verticalAccuracy];
  position.speed = [newLocation speed];
  position.heading = [newLocation course];
  position.timestamp = base::Time::Now();
  position.error_code = Geoposition::ERROR_CODE_NONE;
  dataProvider_->UpdatePosition(&position);
}

- (void)locationManager:(CLLocationManager*)manager
       didFailWithError:(NSError*)error {
  Geoposition position;
  switch ([error code]) {
    case kCLErrorLocationUnknown:
      position.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
      break;
    case kCLErrorDenied:
      position.error_code = Geoposition::ERROR_CODE_PERMISSION_DENIED;
      break;
    default:
      NOTREACHED() << "Unknown CoreLocation error: " << [error code];
      return;
  }
  dataProvider_->UpdatePosition(&position);
}

- (BOOL)loadCoreLocationBundle {
  if (!bundle_) {
    bundle_ = [[NSBundle alloc]
        initWithPath:@"/System/Library/Frameworks/CoreLocation.framework"];
    if (!bundle_) {
      DLOG(WARNING) << "Couldn't load CoreLocation Framework";
      return NO;
    }

    locationManagerClass_ = [bundle_ classNamed:@"CLLocationManager"];
  }

  return YES;
}

@end

CoreLocationDataProviderMac::CoreLocationDataProviderMac() {
  if (MessageLoop::current() !=
      GeolocationProvider::GetInstance()->message_loop()) {
    NOTREACHED() << "CoreLocation data provider must be created on "
        "the Geolocation thread.";
  }
  provider_ = NULL;
  wrapper_.reset([[CoreLocationWrapperMac alloc] initWithDataProvider:this]);
}

CoreLocationDataProviderMac::~CoreLocationDataProviderMac() {
}

// Returns true if the CoreLocation wrapper can load the framework and
// location services are enabled.  The pointer argument will only be accessed
// in the origin thread.
bool CoreLocationDataProviderMac::
    StartUpdating(CoreLocationProviderMac* provider) {
  DCHECK(provider);
  DCHECK(!provider_) << "StartUpdating called twice";
  if (![wrapper_ locationDataAvailable]) return false;
  provider_ = provider;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
     NewRunnableMethod(this, &CoreLocationDataProviderMac::StartUpdatingTask));
  return true;
}

// Clears provider_ so that any leftover messages from CoreLocation get ignored
void CoreLocationDataProviderMac::StopUpdating() {
  provider_ = NULL;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
              NewRunnableMethod(this,
                               &CoreLocationDataProviderMac::StopUpdatingTask));
}

void CoreLocationDataProviderMac::UpdatePosition(Geoposition *position) {
  GeolocationProvider::GetInstance()->message_loop()->PostTask(FROM_HERE,
              NewRunnableMethod(this,
                               &CoreLocationDataProviderMac::PositionUpdated,
                               *position));
}

// Runs in BrowserThread::UI
void CoreLocationDataProviderMac::StartUpdatingTask() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  [wrapper_ startLocation];
}

// Runs in BrowserThread::UI
void CoreLocationDataProviderMac::StopUpdatingTask() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  [wrapper_ stopLocation];
}

void CoreLocationDataProviderMac::PositionUpdated(Geoposition position) {
  DCHECK(MessageLoop::current() ==
         GeolocationProvider::GetInstance()->message_loop());
  if (provider_)
    provider_->SetPosition(&position);
}

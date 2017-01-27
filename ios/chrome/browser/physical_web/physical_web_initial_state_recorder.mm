// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/physical_web/physical_web_initial_state_recorder.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreLocation/CoreLocation.h>

#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/physical_web/physical_web_constants.h"
#include "ios/chrome/browser/pref_names.h"

namespace {

const double kStartupDelaySeconds = 10.0;

// Initial state of settings relevant to the Physical Web feature. These are
// ordered so that bits 2:0 encode the Bluetooth enabled state, the Location
// Services enabled state, and whether Chrome has been granted the Location app
// permission. Bits 4:3 encode the Physical Web preference tristate.
// This enum is used in a user metrics histogram. The states should not be
// reordered or removed.
enum PhysicalWebInitialStateIosChrome {
  OPTOUT_BTOFF_LOCOFF_UNAUTH,
  OPTOUT_BTOFF_LOCOFF_AUTH,
  OPTOUT_BTOFF_LOCON_UNAUTH,
  OPTOUT_BTOFF_LOCON_AUTH,
  OPTOUT_BTON_LOCOFF_UNAUTH,
  OPTOUT_BTON_LOCOFF_AUTH,
  OPTOUT_BTON_LOCON_UNAUTH,
  OPTOUT_BTON_LOCON_AUTH,
  OPTIN_BTOFF_LOCOFF_UNAUTH,
  OPTIN_BTOFF_LOCOFF_AUTH,
  OPTIN_BTOFF_LOCON_UNAUTH,
  OPTIN_BTOFF_LOCON_AUTH,
  OPTIN_BTON_LOCOFF_UNAUTH,
  OPTIN_BTON_LOCOFF_AUTH,
  OPTIN_BTON_LOCON_UNAUTH,
  OPTIN_BTON_LOCON_AUTH,
  ONBOARDING_BTOFF_LOCOFF_UNAUTH,
  ONBOARDING_BTOFF_LOCOFF_AUTH,
  ONBOARDING_BTOFF_LOCON_UNAUTH,
  ONBOARDING_BTOFF_LOCON_AUTH,
  ONBOARDING_BTON_LOCOFF_UNAUTH,
  ONBOARDING_BTON_LOCOFF_AUTH,
  ONBOARDING_BTON_LOCON_UNAUTH,
  ONBOARDING_BTON_LOCON_AUTH,
  PHYSICAL_WEB_INITIAL_STATE_COUNT,

  // Helper flag values
  LOCATION_AUTHORIZED_FLAG = 1 << 0,
  LOCATION_SERVICES_FLAG = 1 << 1,
  BLUETOOTH_FLAG = 1 << 2,
  OPTIN_FLAG = 1 << 3,
  ONBOARDING_FLAG = 1 << 4,
};
}  // namespace

@implementation PhysicalWebInitialStateRecorder {
  int preferenceState_;
  BOOL recordedState_;
  base::scoped_nsobject<NSTimer> startupDelayTimer_;
  base::scoped_nsobject<CBCentralManager> centralManager_;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    preferenceState_ = prefService->GetInteger(prefs::kIosPhysicalWebEnabled);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [self invalidate];
  [super dealloc];
}

- (void)invalidate {
  if (startupDelayTimer_.get()) {
    [startupDelayTimer_ invalidate];
    startupDelayTimer_.reset();
  }
  [centralManager_ setDelegate:nil];
  centralManager_.reset();
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  [centralManager_ setDelegate:nil];
  centralManager_.reset();

  BOOL bluetoothEnabled = [centralManager_ state] == CBManagerStatePoweredOn;

  BOOL locationServicesEnabled = [CLLocationManager locationServicesEnabled];

  CLAuthorizationStatus authStatus = [CLLocationManager authorizationStatus];
  BOOL locationAuthorized =
      authStatus == kCLAuthorizationStatusAuthorizedWhenInUse ||
      authStatus == kCLAuthorizationStatusAuthorizedAlways;

  [self recordStateWithPreferenceState:preferenceState_
                      bluetoothEnabled:bluetoothEnabled
               locationServicesEnabled:locationServicesEnabled
                    locationAuthorized:locationAuthorized];
}

- (void)collectAndRecordState {
  if (recordedState_) {
    return;
  }
  recordedState_ = YES;
  startupDelayTimer_.reset(
      [[NSTimer scheduledTimerWithTimeInterval:kStartupDelaySeconds
                                        target:self
                                      selector:@selector(startupDelayElapsed:)
                                      userInfo:nil
                                       repeats:NO] retain]);
}

- (void)startupDelayElapsed:(NSTimer*)timer {
  startupDelayTimer_.reset();

  // The Bluetooth enabled state must be checked asynchronously. When the state
  // is ready, it will call our centralManagerDidUpdateState method.
  centralManager_.reset([[CBCentralManager alloc]
      initWithDelegate:self
                 queue:dispatch_get_main_queue()
               options:@{
                 // By default, creating a CBCentralManager object with
                 // Bluetooth disabled will prompt the user to enable Bluetooth.
                 // Passing ShowPowerAlert=NO disables the prompt so we can
                 // check the Bluetooth enabled state silently.
                 CBCentralManagerOptionShowPowerAlertKey : @NO
               }]);
}

- (void)recordStateWithPreferenceState:(int)preferenceState
                      bluetoothEnabled:(BOOL)bluetoothEnabled
               locationServicesEnabled:(BOOL)locationServicesEnabled
                    locationAuthorized:(BOOL)locationAuthorized {
  int state = 0;
  if (preferenceState == physical_web::kPhysicalWebOn) {
    state |= OPTIN_FLAG;
  } else if (preferenceState == physical_web::kPhysicalWebOnboarding) {
    state |= ONBOARDING_FLAG;
  }
  if (locationServicesEnabled) {
    state |= LOCATION_SERVICES_FLAG;
  }
  if (locationAuthorized) {
    state |= LOCATION_AUTHORIZED_FLAG;
  }
  if (bluetoothEnabled) {
    state |= BLUETOOTH_FLAG;
  }

  DCHECK(state < PHYSICAL_WEB_INITIAL_STATE_COUNT);
  UMA_HISTOGRAM_ENUMERATION("PhysicalWeb.InitialState.IosChrome", state,
                            PHYSICAL_WEB_INITIAL_STATE_COUNT);
}

@end

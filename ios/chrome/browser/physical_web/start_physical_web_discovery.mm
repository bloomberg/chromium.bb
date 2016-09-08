// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/physical_web/start_physical_web_discovery.h"

#import <CoreLocation/CoreLocation.h>

#include "components/physical_web/data_source/physical_web_data_source.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/physical_web/physical_web_constants.h"
#include "ios/chrome/browser/pref_names.h"

void StartPhysicalWebDiscovery(PrefService* pref_service) {
  // Do not scan if the Physical Web feature is disabled by a command line flag
  // or Chrome Variations experiment.
  if (!experimental_flags::IsPhysicalWebEnabled()) {
    return;
  }

  // Do not scan for nearby devices if the user has explicitly disabled the
  // Physical Web feature.
  int preference_state =
      pref_service->GetInteger(prefs::kIosPhysicalWebEnabled);
  if (preference_state == physical_web::kPhysicalWebOff) {
    return;
  }

  // In the default (onboarding) state, enable Physical Web scanning if the user
  // has granted the Location app permission.
  if (preference_state == physical_web::kPhysicalWebOnboarding) {
    CLAuthorizationStatus auth_status = [CLLocationManager authorizationStatus];
    if (auth_status != kCLAuthorizationStatusAuthorizedWhenInUse &&
        auth_status != kCLAuthorizationStatusAuthorizedAlways) {
      return;
    }
    pref_service->SetInteger(prefs::kIosPhysicalWebEnabled, true);
  }

  GetApplicationContext()->GetPhysicalWebDataSource()->StartDiscovery(true);
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/physical_web/start_physical_web_discovery.h"

#import <CoreLocation/CoreLocation.h>

#include "components/physical_web/data_source/physical_web_data_source.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_config.h"
#include "ios/chrome/browser/physical_web/physical_web_constants.h"
#include "ios/chrome/browser/pref_names.h"
#include "url/gurl.h"

void StartPhysicalWebDiscovery(PrefService* pref_service, bool is_incognito) {
  // Do not scan if the Physical Web feature is disabled by a command line flag
  // or Chrome Variations experiment.
  if (!experimental_flags::IsPhysicalWebEnabled()) {
    return;
  }

  // If the preference is in the default state, check if conditions allow us to
  // auto-enable it. The preference can be auto-enabled if the omnibox would
  // send the X-Geo geolocation header in search queries to www.google.com.
  int preference_state =
      pref_service->GetInteger(prefs::kIosPhysicalWebEnabled);
  if (preference_state == physical_web::kPhysicalWebOnboarding) {
    // Check that Location Services is enabled.
    bool location_services_enabled =
        [CLLocationManager locationServicesEnabled];

    // Check that the user has authorized Chrome to use location.
    CLAuthorizationStatus auth_status = [CLLocationManager authorizationStatus];
    bool location_authorized =
        auth_status == kCLAuthorizationStatusAuthorizedWhenInUse ||
        auth_status == kCLAuthorizationStatusAuthorizedAlways;

    // Check that the user has not revoked the geolocation permission for
    // Google.
    bool geolocation_eligible = [[OmniboxGeolocationConfig sharedInstance]
        URLHasEligibleDomain:GURL("https://www.google.com")];

    if (!is_incognito && location_services_enabled && location_authorized &&
        geolocation_eligible) {
      pref_service->SetInteger(prefs::kIosPhysicalWebEnabled,
                               physical_web::kPhysicalWebOn);
      preference_state =
          pref_service->GetInteger(prefs::kIosPhysicalWebEnabled);
    }
  }

  // Scan only if the feature is enabled.
  if (preference_state == physical_web::kPhysicalWebOn) {
    GetApplicationContext()->GetPhysicalWebDataSource()->StartDiscovery(true);
  } else {
    GetApplicationContext()->GetPhysicalWebDataSource()->StopDiscovery();
  }
}

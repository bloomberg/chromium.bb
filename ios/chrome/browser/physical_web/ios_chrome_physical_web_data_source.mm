// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/physical_web/ios_chrome_physical_web_data_source.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/physical_web/physical_web_initial_state_recorder.h"
#import "ios/chrome/common/physical_web/physical_web_scanner.h"

IOSChromePhysicalWebDataSource::IOSChromePhysicalWebDataSource(
    PrefService* pref_service) {
  initialStateRecorder_.reset([[PhysicalWebInitialStateRecorder alloc]
      initWithPrefService:pref_service]);
  [initialStateRecorder_ collectAndRecordState];
}

IOSChromePhysicalWebDataSource::~IOSChromePhysicalWebDataSource() {
  [initialStateRecorder_ invalidate];
  StopDiscovery();
}

void IOSChromePhysicalWebDataSource::StartDiscovery(
    bool network_request_enabled) {
  // If there are unresolved beacons it means the scanner is started but does
  // not have network requests enabled. In this case we should avoid recreating
  // the scanner as it would clear the cache of nearby beacons.
  if (network_request_enabled && HasUnresolvedDiscoveries()) {
    [scanner_ setNetworkRequestEnabled:YES];
    return;
  }

  [scanner_ stop];
  scanner_.reset([[PhysicalWebScanner alloc] initWithDelegate:nil]);

  // Configure the scanner to notify us when a URL is no longer nearby.
  [scanner_ setOnLostDetectionEnabled:YES];
  [scanner_ setNetworkRequestEnabled:network_request_enabled];
  [scanner_ start];
}

void IOSChromePhysicalWebDataSource::StopDiscovery() {
  [scanner_ stop];
  scanner_.reset();
}

std::unique_ptr<physical_web::MetadataList>
IOSChromePhysicalWebDataSource::GetMetadataList() {
  if (!scanner_) {
    return base::MakeUnique<physical_web::MetadataList>();
  }
  return [scanner_ metadataList];
}

bool IOSChromePhysicalWebDataSource::HasUnresolvedDiscoveries() {
  return [scanner_ unresolvedBeaconsCount] > 0;
}

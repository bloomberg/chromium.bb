// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_SCANNER_H_
#define IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_SCANNER_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <vector>

namespace physical_web {
struct Metadata;
using MetadataList = std::vector<Metadata>;
}

@protocol PhysicalWebScannerDelegate;

// This class will scan for physical web devices.

@interface PhysicalWebScanner : NSObject

// When onLostDetectionEnabled is YES, the delegate will be notified if a URL
// that was previously nearby is no longer detected. Changing this value while
// scanning is active will reset the list of nearby URLs.
@property(nonatomic, assign) BOOL onLostDetectionEnabled;

// When networkRequest is NO, no network request will be sent.
@property(nonatomic, assign) BOOL networkRequestEnabled;

// When networkRequest is NO, it returns the number of discovered beacons.
@property(nonatomic, readonly) int unresolvedBeaconsCount;

// Whether bluetooth is enabled. Must not be read before the first call of
// scannerBluetoothStatusUpdated.
@property(nonatomic, readonly) BOOL bluetoothEnabled;

// Initializes a physical web device scanner.
- (instancetype)initWithDelegate:(id<PhysicalWebScannerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Start scanning and clear the list of discovered devices. The scanning might
// not start effectively immediately.
// It can happen when the bluetooth is turned off. In this case, scanning will
// start as soon as the bluetooth is turned on.
- (void)start;

// Stop scanning.
- (void)stop;

// Returns a list of physical web devices (PhysicalWebDevice).
- (NSArray*)devices;

// Returns the metadata for all resolved physical web URLs. The returned value
// will never be null; if no metadata has been received then an empty list is
// returned.
- (std::unique_ptr<physical_web::MetadataList>)metadataList;

@end

@protocol PhysicalWebScannerDelegate<NSObject>

// This delegate method is called when the list of discovered devices is updated
// by the scanner.
- (void)scannerUpdatedDevices:(PhysicalWebScanner*)scanner;

// This delegate method is called when the bluetooth status is updated.
- (void)scannerBluetoothStatusUpdated:(PhysicalWebScanner*)scanner;

@end

#endif  // IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_SCANNER_H_

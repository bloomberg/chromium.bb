// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/physical_web/physical_web_scanner.h"

#include <string>
#include <vector>

#import <CoreBluetooth/CoreBluetooth.h>

#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/uribeacon/uri_encoder.h"
#include "ios/chrome/common/physical_web/physical_web_device.h"
#import "ios/chrome/common/physical_web/physical_web_request.h"
#include "ios/chrome/common/physical_web/physical_web_types.h"

namespace {

NSString* const kUriBeaconServiceUUID = @"FED8";
NSString* const kEddystoneBeaconServiceUUID = @"FEAA";

enum BeaconType {
  BEACON_TYPE_NONE,
  BEACON_TYPE_URIBEACON,
  BEACON_TYPE_EDDYSTONE,
};

}  // namespace

@interface PhysicalWebScanner ()<CBCentralManagerDelegate>

// Decodes the UriBeacon information in the given |data| and a beacon |type| to
// return an unresolved PhysicalWebDevice instance. It also stores the given
// |rssi| in the result.
+ (PhysicalWebDevice*)newDeviceFromData:(NSData*)data
                                   rssi:(int)rssi
                                   type:(BeaconType)type;
// Starts the CoreBluetooth scanner when the bluetooth is powered on.
- (void)reallyStart;
// Requests metadata of a device if the same URL has not been requested before.
- (void)requestMetadataForDevice:(PhysicalWebDevice*)device;
// Returns the beacon type given the advertisement data.
+ (BeaconType)beaconTypeForAdvertisementData:(NSDictionary*)advertisementData;

@end

@implementation PhysicalWebScanner {
  // Delegate that will be notified when the list of devices change.
  id<PhysicalWebScannerDelegate> delegate_;
  // The value of |started_| is YES when the scanner has been started and NO
  // when it's been stopped. The initial value is NO.
  BOOL started_;
  // The value is valid when the scanner has been started. If bluetooth is not
  // powered on, the value is YES, if it's powered on and the CoreBluetooth
  // scanner has started, the value is NO.
  BOOL pendingStart_;
  // List of PhysicalWebRequest that we're waiting the response from.
  base::scoped_nsobject<NSMutableArray> pendingRequests_;
  // List of resolved PhysicalWebDevice.
  base::scoped_nsobject<NSMutableArray> devices_;
  // List of URLs that have been resolved or have a pending resolution from a
  // PhysicalWebRequest.
  base::scoped_nsobject<NSMutableSet> devicesUrls_;
  // List of final URLs that have been resolved. This set will help us
  // deduplicate the final URLs.
  base::scoped_nsobject<NSMutableSet> finalUrls_;
  // CoreBluetooth scanner.
  base::scoped_nsobject<CBCentralManager> centralManager_;
  // The value is YES if network requests can be sent.
  BOOL networkRequestEnabled_;
  // List of unresolved PhysicalWebDevice when network requests are not enabled.
  base::scoped_nsobject<NSMutableArray> unresolvedDevices_;
}

@synthesize networkRequestEnabled = networkRequestEnabled_;

- (instancetype)initWithDelegate:(id<PhysicalWebScannerDelegate>)delegate {
  self = [super init];
  if (self) {
    delegate_ = delegate;
    devices_.reset([[NSMutableArray alloc] init]);
    devicesUrls_.reset([[NSMutableSet alloc] init]);
    finalUrls_.reset([[NSMutableSet alloc] init]);
    pendingRequests_.reset([[NSMutableArray alloc] init]);
    centralManager_.reset([[CBCentralManager alloc]
        initWithDelegate:self
                   queue:dispatch_get_main_queue()]);
    unresolvedDevices_.reset([[NSMutableArray alloc] init]);
    [[NSHTTPCookieStorage sharedHTTPCookieStorage]
        setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyNever];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [centralManager_ setDelegate:nil];
  centralManager_.reset();
  [super dealloc];
}

- (void)start {
  [self stop];
  [finalUrls_ removeAllObjects];
  [devicesUrls_ removeAllObjects];
  [devices_ removeAllObjects];
  started_ = YES;
  if ([self bluetoothEnabled])
    [self reallyStart];
  else
    pendingStart_ = YES;
}

- (void)stop {
  if (!started_)
    return;
  for (PhysicalWebRequest* request in pendingRequests_.get()) {
    [request cancel];
  }
  [pendingRequests_ removeAllObjects];
  if (!pendingStart_ && [self bluetoothEnabled]) {
    [centralManager_ stopScan];
  }
  pendingStart_ = NO;
  started_ = NO;
}

- (NSArray*)devices {
  return [devices_ sortedArrayUsingComparator:^(id obj1, id obj2) {
    PhysicalWebDevice* device1 = obj1;
    PhysicalWebDevice* device2 = obj2;
    // Sorts in ascending order.
    if ([device1 rank] > [device2 rank]) {
      return NSOrderedDescending;
    }
    if ([device1 rank] < [device2 rank]) {
      return NSOrderedAscending;
    }
    return NSOrderedSame;
  }];
}

- (void)setNetworkRequestEnabled:(BOOL)enabled {
  if (networkRequestEnabled_ == enabled) {
    return;
  }
  networkRequestEnabled_ = enabled;
  if (!networkRequestEnabled_)
    return;

  // Sends the pending requests.
  for (PhysicalWebDevice* device in unresolvedDevices_.get()) {
    [self requestMetadataForDevice:device];
  }
  [unresolvedDevices_ removeAllObjects];
}

- (int)unresolvedBeaconsCount {
  return [unresolvedDevices_ count];
}

- (BOOL)bluetoothEnabled {
// TODO(crbug.com/619982): The CBManager base class appears to still be in
// flux.  Unwind this #ifdef once the APIs settle.
#if defined(__IPHONE_10_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
  return [centralManager_ state] == CBManagerStatePoweredOn;
#else
  return [centralManager_ state] == CBCentralManagerStatePoweredOn;
#endif
}

- (void)reallyStart {
  pendingStart_ = NO;
  NSArray* serviceUUIDs = @[
    [CBUUID UUIDWithString:kUriBeaconServiceUUID],
    [CBUUID UUIDWithString:kEddystoneBeaconServiceUUID]
  ];
  [centralManager_ scanForPeripheralsWithServices:serviceUUIDs options:nil];
}

#pragma mark -
#pragma mark CBCentralManagerDelegate methods

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  if ([self bluetoothEnabled]) {
    if (pendingStart_)
      [self reallyStart];
  } else {
    if (started_ && !pendingStart_) {
      pendingStart_ = YES;
      [centralManager_ stopScan];
    }
  }
  [delegate_ scannerBluetoothStatusUpdated:self];
}

+ (BeaconType)beaconTypeForAdvertisementData:(NSDictionary*)advertisementData {
  NSDictionary* serviceData =
      [advertisementData objectForKey:CBAdvertisementDataServiceDataKey];
  if ([serviceData objectForKey:[CBUUID UUIDWithString:kUriBeaconServiceUUID]])
    return BEACON_TYPE_URIBEACON;
  if ([serviceData
          objectForKey:[CBUUID UUIDWithString:kEddystoneBeaconServiceUUID]])
    return BEACON_TYPE_EDDYSTONE;
  return BEACON_TYPE_NONE;
}

- (void)centralManager:(CBCentralManager*)central
    didDiscoverPeripheral:(CBPeripheral*)peripheral
        advertisementData:(NSDictionary*)advertisementData
                     RSSI:(NSNumber*)RSSI {
  BeaconType type =
      [PhysicalWebScanner beaconTypeForAdvertisementData:advertisementData];
  if (type == BEACON_TYPE_NONE)
    return;

  NSDictionary* serviceData =
      [advertisementData objectForKey:CBAdvertisementDataServiceDataKey];
  NSData* data = nil;
  switch (type) {
    case BEACON_TYPE_URIBEACON:
      data = [serviceData
          objectForKey:[CBUUID UUIDWithString:kUriBeaconServiceUUID]];
      break;
    case BEACON_TYPE_EDDYSTONE:
      data = [serviceData
          objectForKey:[CBUUID UUIDWithString:kEddystoneBeaconServiceUUID]];
      break;
    default:
      // Do nothing.
      break;
  }
  DCHECK(data);

  base::scoped_nsobject<PhysicalWebDevice> device([PhysicalWebScanner
      newDeviceFromData:data
                   rssi:[RSSI intValue]
                   type:type]);
  // Skip if the data couldn't be parsed.
  if (!device.get())
    return;

  // Skip if the URL has already been seen.
  if ([devicesUrls_ containsObject:[device url]])
    return;
  [devicesUrls_ addObject:[device url]];

  if (networkRequestEnabled_) {
    [self requestMetadataForDevice:device];
  } else {
    [unresolvedDevices_ addObject:device];
    [delegate_ scannerUpdatedDevices:self];
  }
}

#pragma mark -
#pragma mark UriBeacon resolution

+ (PhysicalWebDevice*)newDeviceFromData:(NSData*)data
                                   rssi:(int)rssi
                                   type:(BeaconType)type {
  // No UriBeacon service data.
  if (!data)
    return nil;
  // UriBeacon service data too small.
  if ([data length] <= 2)
    return nil;

  const uint8_t* bytes = static_cast<const uint8_t*>([data bytes]);
  if (type == BEACON_TYPE_EDDYSTONE) {
    // The packet type is encoded in the high-order 4 bits.
    // Returns if it's not an Eddystone-URL.
    if ((bytes[0] & 0xf0) != 0x10)
      return nil;
  }

  // - transmit power is at offset 1
  // TX Power in the UriBeacon advertising packet is the received power at 0
  // meters. The Transmit Power Level represents the transmit power level in
  // dBm, and the value ranges from -100 dBm to +20 dBm to a resolution of 1
  // dBm.
  int transmitPower = static_cast<char>(bytes[1]);
  // - scheme and URL are at offset 2.
  std::vector<uint8_t> encodedURI(&bytes[2], &bytes[[data length]]);
  std::string utf8URI;
  device::DecodeUriBeaconUri(encodedURI, utf8URI);
  NSString* uriString = base::SysUTF8ToNSString(utf8URI);
  return [[PhysicalWebDevice alloc] initWithURL:[NSURL URLWithString:uriString]
                                     requestURL:nil
                                           icon:nil
                                          title:nil
                                    description:nil
                                  transmitPower:transmitPower
                                           rssi:rssi
                                           rank:physical_web::kMaxRank];
}

- (void)requestMetadataForDevice:(PhysicalWebDevice*)device {
  base::scoped_nsobject<PhysicalWebRequest> request(
      [[PhysicalWebRequest alloc] initWithDevice:device]);
  PhysicalWebRequest* strongRequest = request.get();
  [pendingRequests_ addObject:strongRequest];
  base::WeakNSObject<PhysicalWebScanner> weakSelf(self);
  [request start:^(PhysicalWebDevice* device, NSError* error) {
    base::scoped_nsobject<PhysicalWebScanner> strongSelf([weakSelf retain]);
    if (!strongSelf) {
      return;
    }
    // ignore if there's an error.
    if (!error) {
      if (![strongSelf.get()->finalUrls_ containsObject:[device url]]) {
        [strongSelf.get()->devices_ addObject:device];
        [strongSelf.get()->delegate_ scannerUpdatedDevices:weakSelf];
        [strongSelf.get()->finalUrls_ addObject:[device url]];
      }
    }
    [strongSelf.get()->pendingRequests_ removeObject:strongRequest];
  }];
}

@end

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/physical_web/physical_web_scanner.h"

#import <CoreBluetooth/CoreBluetooth.h>

#include <string>
#include <vector>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "device/bluetooth/uribeacon/uri_encoder.h"
#import "ios/chrome/common/physical_web/physical_web_device.h"
#import "ios/chrome/common/physical_web/physical_web_request.h"
#import "ios/chrome/common/physical_web/physical_web_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kUriBeaconServiceUUID = @"FED8";
NSString* const kEddystoneBeaconServiceUUID = @"FEAA";

// The length of time in seconds since a URL was last seen before it should be
// considered lost (ie, no longer nearby).
const NSTimeInterval kLostThresholdSeconds = 15.0;
// The time interval in seconds between checks for lost URLs.
const NSTimeInterval kUpdateIntervalSeconds = 6.0;

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
// Starts the CoreBluetooth scanner when the bluetooth is powered on and starts
// the update timer.
- (void)reallyStart;
// Stops the CoreBluetooth scanner and update timer.
- (void)reallyStop;
// Timer callback to check for lost URLs based on the elapsed time since they
// were last seen.
- (void)onUpdateTimeElapsed:(NSTimer*)timer;
// Requests metadata of a device if the same URL has not been requested before.
- (void)requestMetadataForDevice:(PhysicalWebDevice*)device;
// Returns the beacon type given the advertisement data.
+ (BeaconType)beaconTypeForAdvertisementData:(NSDictionary*)advertisementData;

@end

@implementation PhysicalWebScanner {
  // Delegate that will be notified when the list of devices change.
  __unsafe_unretained id<PhysicalWebScannerDelegate> delegate_;
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
  // When YES, we will notify the delegate if a previously nearby URL is lost
  // and remove it from the list of nearby devices.
  BOOL onLostDetectionEnabled_;
  // The value is YES if network requests can be sent.
  BOOL networkRequestEnabled_;
  // List of unresolved PhysicalWebDevice when network requests are not enabled.
  base::scoped_nsobject<NSMutableArray> unresolvedDevices_;
  // A repeating timer to check for lost URLs. If the elapsed time since an URL
  // was last seen exceeds a threshold, the URL is considered lost.
  base::scoped_nsobject<NSTimer> updateTimer_;
}

@synthesize onLostDetectionEnabled = onLostDetectionEnabled_;
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
                   queue:dispatch_get_main_queue()
                 options:@{
                   CBCentralManagerOptionShowPowerAlertKey : @NO
                 }]);
    unresolvedDevices_.reset([[NSMutableArray alloc] init]);
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
  if (updateTimer_.get()) {
    [updateTimer_ invalidate];
    updateTimer_.reset();
  }
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
    [self reallyStop];
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

- (std::unique_ptr<base::ListValue>)metadata {
  auto metadataList = base::MakeUnique<base::ListValue>();

  for (PhysicalWebDevice* device in [self devices]) {
    std::string scannedUrl =
        base::SysNSStringToUTF8([[device requestURL] absoluteString]);
    std::string resolvedUrl =
        base::SysNSStringToUTF8([[device url] absoluteString]);
    std::string icon = base::SysNSStringToUTF8([[device icon] absoluteString]);
    std::string title = base::SysNSStringToUTF8([device title]);
    std::string description = base::SysNSStringToUTF8([device description]);

    auto metadataItem = base::MakeUnique<base::DictionaryValue>();
    metadataItem->SetString(physical_web::kScannedUrlKey, scannedUrl);
    metadataItem->SetString(physical_web::kResolvedUrlKey, resolvedUrl);
    metadataItem->SetString(physical_web::kIconUrlKey, icon);
    metadataItem->SetString(physical_web::kTitleKey, title);
    metadataItem->SetString(physical_web::kDescriptionKey, description);
    metadataList->Append(std::move(metadataItem));
  }

  return metadataList;
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

- (void)setOnLostDetectionEnabled:(BOOL)enabled {
  if (enabled == onLostDetectionEnabled_) {
    return;
  }
  onLostDetectionEnabled_ = enabled;
  if (started_) {
    [self start];
  }
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

  if (updateTimer_.get()) {
    [updateTimer_ invalidate];
    updateTimer_.reset();
  }

  NSArray* serviceUUIDs = @[
    [CBUUID UUIDWithString:kUriBeaconServiceUUID],
    [CBUUID UUIDWithString:kEddystoneBeaconServiceUUID]
  ];
  if (onLostDetectionEnabled_) {
    // Register a repeating timer to periodically check for lost URLs.
    updateTimer_.reset([NSTimer
        scheduledTimerWithTimeInterval:kUpdateIntervalSeconds
                                target:self
                              selector:@selector(onUpdateTimeElapsed:)
                              userInfo:nil
                               repeats:YES]);
  }
  [centralManager_ scanForPeripheralsWithServices:serviceUUIDs options:nil];
}

- (void)reallyStop {
  if (updateTimer_.get()) {
    [updateTimer_ invalidate];
    updateTimer_.reset();
  }

  [centralManager_ stopScan];
}

- (void)onUpdateTimeElapsed:(NSTimer*)timer {
  NSDate* now = [NSDate date];
  NSMutableArray* lostDevices = [NSMutableArray array];
  NSMutableArray* lostUnresolvedDevices = [NSMutableArray array];
  NSMutableArray* lostScannedUrls = [NSMutableArray array];

  for (PhysicalWebDevice* device in devices_.get()) {
    NSDate* scanTimestamp = [device scanTimestamp];
    NSTimeInterval elapsedSeconds = [now timeIntervalSinceDate:scanTimestamp];
    if (elapsedSeconds > kLostThresholdSeconds) {
      [lostDevices addObject:device];
      [lostScannedUrls addObject:[device requestURL]];
      [devicesUrls_ removeObject:[device requestURL]];
      [finalUrls_ removeObject:[device url]];
    }
  }

  for (PhysicalWebDevice* device in unresolvedDevices_.get()) {
    NSDate* scanTimestamp = [device scanTimestamp];
    NSTimeInterval elapsedSeconds = [now timeIntervalSinceDate:scanTimestamp];
    if (elapsedSeconds > kLostThresholdSeconds) {
      [lostUnresolvedDevices addObject:device];
      [lostScannedUrls addObject:[device requestURL]];
      [devicesUrls_ removeObject:[device requestURL]];
    }
  }

  NSMutableArray* requestsToRemove = [NSMutableArray array];
  for (PhysicalWebRequest* request in pendingRequests_.get()) {
    if ([lostScannedUrls containsObject:[request requestURL]]) {
      [request cancel];
      [requestsToRemove addObject:request];
    }
  }

  [devices_ removeObjectsInArray:lostDevices];
  [unresolvedDevices_ removeObjectsInArray:lostUnresolvedDevices];
  [pendingRequests_ removeObjectsInArray:requestsToRemove];

  if ([lostDevices count]) {
    [delegate_ scannerUpdatedDevices:self];
  }

  // TODO(crbug.com/657056): Remove this workaround when radar is fixed.
  // For unknown reasons, when scanning for longer periods (on the order of
  // minutes), the scanner is less reliable at detecting all nearby URLs. As a
  // workaround, we restart the scanner each time we check for lost URLs.
  NSArray* serviceUUIDs = @[
    [CBUUID UUIDWithString:kUriBeaconServiceUUID],
    [CBUUID UUIDWithString:kEddystoneBeaconServiceUUID]
  ];
  [centralManager_ stopScan];
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
      [self reallyStop];
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

  // If the URL has already been seen, update its timestamp.
  if ([devicesUrls_ containsObject:[device requestURL]]) {
    for (PhysicalWebDevice* unresolvedDevice in unresolvedDevices_.get()) {
      if ([[unresolvedDevice requestURL] isEqual:[device requestURL]]) {
        [unresolvedDevice setScanTimestamp:[NSDate date]];
        return;
      }
    }
    for (PhysicalWebDevice* resolvedDevice in devices_.get()) {
      if ([[resolvedDevice requestURL] isEqual:[device requestURL]]) {
        [resolvedDevice setScanTimestamp:[NSDate date]];
        break;
      }
    }
    return;
  }

  [device setScanTimestamp:[NSDate date]];
  [devicesUrls_ addObject:[device requestURL]];

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
  NSURL* url = [NSURL URLWithString:uriString];

  // Ensure URL is valid.
  if (!url)
    return nil;

  return [[PhysicalWebDevice alloc] initWithURL:url
                                     requestURL:url
                                           icon:nil
                                          title:nil
                                    description:nil
                                  transmitPower:transmitPower
                                           rssi:rssi
                                           rank:physical_web::kMaxRank
                                  scanTimestamp:[NSDate date]];
}

- (void)requestMetadataForDevice:(PhysicalWebDevice*)device {
  base::scoped_nsobject<PhysicalWebRequest> request(
      [[PhysicalWebRequest alloc] initWithDevice:device]);
  PhysicalWebRequest* strongRequest = request.get();
  [pendingRequests_ addObject:strongRequest];
  base::WeakNSObject<PhysicalWebScanner> weakSelf(self);
  [request start:^(PhysicalWebDevice* device, NSError* error) {
    base::scoped_nsobject<PhysicalWebScanner> strongSelf(weakSelf);
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

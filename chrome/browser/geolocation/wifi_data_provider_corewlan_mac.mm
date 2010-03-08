// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements a WLAN API binding for CoreWLAN, as available on OSX 10.6

#include "chrome/browser/geolocation/wifi_data_provider_mac.h"

#import <Foundation/Foundation.h>

#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_nsobject.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
// If building on 10.6 we can include the framework header directly.
#import <CoreWLAN/CoreWLAN.h>

#else
// Otherwise just define the interfaces we require. Class definitions required
// as we treat warnings as errors so we can't pass message to an untyped id.

@interface CWInterface : NSObject
+ (CWInterface*)interface;
- (NSArray*)scanForNetworksWithParameters:(NSDictionary*)parameters
                                    error:(NSError**)error;
@end

@interface CWNetwork : NSObject <NSCopying, NSCoding>
@property(readonly) NSString* ssid;
@property(readonly) NSString* bssid;
@property(readonly) NSData* bssidData;
@property(readonly) NSNumber* securityMode;
@property(readonly) NSNumber* phyMode;
@property(readonly) NSNumber* channel;
@property(readonly) NSNumber* rssi;
@property(readonly) NSNumber* noise;
@property(readonly) NSData* ieData;
@property(readonly) BOOL isIBSS;
- (BOOL)isEqualToNetwork:(CWNetwork*)network;
@end

// String literals derived empirically on an OSX 10.6 machine (XCode 3.2.1).
// Commented out to avoid unused variables warnings; comment back in as needed.

// static const NSString* kCWAssocKey8021XProfile = @"ASSOC_KEY_8021X_PROFILE";
// static const NSString* kCWAssocKeyPassphrase = @"ASSOC_KEY_PASSPHRASE";
// static const NSString* kCWBSSIDDidChangeNotification =
//     @"BSSID_CHANGED_NOTIFICATION";
// static const NSString* kCWCountryCodeDidChangeNotification =
//     @"COUNTRY_CODE_CHANGED_NOTIFICATION";
// static const NSString* kCWErrorDomain = @"APPLE80211_ERROR_DOMAIN";
// static const NSString* kCWIBSSKeyChannel = @"IBSS_KEY_CHANNEL";
// static const NSString* kCWIBSSKeyPassphrase = @"IBSS_KEY_PASSPHRASE";
// static const NSString* kCWIBSSKeySSID = @"IBSS_KEY_SSID";
// static const NSString* kCWLinkDidChangeNotification =
//     @"LINK_CHANGED_NOTIFICATION";
// static const NSString* kCWModeDidChangeNotification =
//     @"MODE_CHANGED_NOTIFICATION";
// static const NSString* kCWPowerDidChangeNotification =
//     @"POWER_CHANGED_NOTIFICATION";
// static const NSString* kCWScanKeyBSSID = @"BSSID";
// static const NSString* kCWScanKeyDwellTime = @"SCAN_DWELL_TIME";
static const NSString* kCWScanKeyMerge = @"SCAN_MERGE";
// static const NSString* kCWScanKeyRestTime = @"SCAN_REST_TIME";
// static const NSString* kCWScanKeyScanType = @"SCAN_TYPE";
// static const NSString* kCWScanKeySSID = @"SSID_STR";
// static const NSString* kCWSSIDDidChangeNotification =
//     @"SSID_CHANGED_NOTIFICATION";

#endif  // MAC_OS_X_VERSION_MAX_ALLOWED >= 1060

class CoreWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  CoreWlanApi() {}

  // Must be called before any other interface method. Will return false if the
  // CoreWLAN framework cannot be initialized (e.g. running on pre-10.6 OSX),
  // in which case no other method may be called.
  bool Init();

  // WlanApiInterface
  virtual bool GetAccessPointData(WifiData::AccessPointDataSet* data);

 private:
  scoped_nsobject<NSBundle> bundle_;
  scoped_nsobject<CWInterface> corewlan_interface_;

  DISALLOW_COPY_AND_ASSIGN(CoreWlanApi);
};

bool CoreWlanApi::Init() {
  // As the WLAN api binding runs on its own thread, we need to provide our own
  // auto release pool. It's simplest to do this as an automatic variable in
  // each method that needs it, to ensure the scoping is correct and does not
  // interfere with any other code using autorelease pools on the thread.
  base::ScopedNSAutoreleasePool auto_pool;
  bundle_.reset([[NSBundle alloc]
      initWithPath:@"/System/Library/Frameworks/CoreWLAN.framework"]);
  if (!bundle_) {
    DLOG(INFO) << "Failed to load the CoreWLAN framework bundle";
    return false;
  }
  corewlan_interface_.reset([[bundle_ classNamed:@"CWInterface"] interface]);
  if (!corewlan_interface_) {
    DLOG(INFO) << "Failed to create the CWInterface instance";
    return false;
  }
  [corewlan_interface_ retain];
  return true;
}

bool CoreWlanApi::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  base::ScopedNSAutoreleasePool auto_pool;
  DCHECK(corewlan_interface_);
  NSError* err = nil;
  // Initialize the scan parameters with scan key merging disabled, so we get
  // every AP listed in the scan without any SSID de-duping logic.
  NSDictionary* params =
      [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:NO]
                                  forKey:kCWScanKeyMerge];

  NSArray* scan = [corewlan_interface_ scanForNetworksWithParameters:params
                                                               error:&err];

  const int error_code = [err code];
  const int count = [scan count];
  if (error_code && !count) {
    DLOG(WARNING) << "CoreWLAN scan failed " << error_code;
    return false;
  }
  DLOG(INFO) << "Found " << count << " wifi APs";

  for (CWNetwork* network in scan) {
    DCHECK(network);
    AccessPointData access_point_data;
    NSData* mac = [network bssidData];
    DCHECK([mac length] == 6);
    access_point_data.mac_address = MacAddressAsString16(
        static_cast<const uint8*>([mac bytes]));
    access_point_data.radio_signal_strength = [[network rssi] intValue];
    access_point_data.channel = [[network channel] intValue];
    access_point_data.signal_to_noise =
        access_point_data.radio_signal_strength - [[network noise] intValue];
    access_point_data.ssid = UTF8ToUTF16([[network ssid] UTF8String]);
    data->insert(access_point_data);
  }
  return true;
}

WifiDataProviderCommon::WlanApiInterface* NewCoreWlanApi() {
  scoped_ptr<CoreWlanApi> self(new CoreWlanApi);
  if (self->Init())
    return self.release();

  return NULL;
}

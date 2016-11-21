// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/physical_web/physical_web_device.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/common/physical_web/physical_web_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PhysicalWebDevice {
  base::scoped_nsobject<NSURL> url_;
  base::scoped_nsobject<NSURL> requestURL_;
  base::scoped_nsobject<NSURL> icon_;
  base::scoped_nsobject<NSString> title_;
  base::scoped_nsobject<NSString> description_;
  int rssi_;
  int transmitPower_;
  double rank_;
  base::scoped_nsobject<NSDate> scanTimestamp_;
}

@synthesize rssi = rssi_;
@synthesize transmitPower = transmitPower_;
@synthesize rank = rank_;

- (instancetype)initWithURL:(NSURL*)url
                 requestURL:(NSURL*)requestURL
                       icon:(NSURL*)icon
                      title:(NSString*)title
                description:(NSString*)description
              transmitPower:(int)transmitPower
                       rssi:(int)rssi
                       rank:(double)rank
              scanTimestamp:(NSDate*)scanTimestamp {
  self = [super init];
  if (self) {
    url_.reset(url);
    requestURL_.reset(requestURL);
    icon_.reset(icon);
    title_.reset([title copy]);
    description_.reset([description copy]);
    transmitPower_ = transmitPower;
    rssi_ = rssi;
    rank_ = rank > physical_web::kMaxRank ? physical_web::kMaxRank : rank;
    scanTimestamp_.reset(scanTimestamp);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (NSURL*)url {
  return url_;
}

- (NSURL*)requestURL {
  return requestURL_;
}

- (NSURL*)icon {
  return icon_;
}

- (NSString*)title {
  return title_;
}

- (NSString*)description {
  return description_;
}

- (NSDate*)scanTimestamp {
  return scanTimestamp_;
}

- (void)setScanTimestamp:(NSDate*)value {
  scanTimestamp_.reset(value);
}

@end

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/physical_web/physical_web_device.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/common/physical_web/physical_web_types.h"

@implementation PhysicalWebDevice {
  base::scoped_nsobject<NSURL> url_;
  base::scoped_nsobject<NSURL> requestURL_;
  base::scoped_nsobject<NSURL> icon_;
  base::scoped_nsobject<NSString> title_;
  base::scoped_nsobject<NSString> description_;
  int rssi_;
  int transmitPower_;
  double rank_;
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
                       rank:(double)rank {
  self = [super init];
  if (self) {
    url_.reset([url retain]);
    requestURL_.reset([requestURL retain]);
    icon_.reset([icon retain]);
    title_.reset([title copy]);
    description_.reset([description copy]);
    transmitPower_ = transmitPower;
    rssi_ = rssi;
    rank_ = rank > physical_web::kMaxRank ? physical_web::kMaxRank : rank;
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

@end

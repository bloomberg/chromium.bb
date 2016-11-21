// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_DEVICE_H_
#define IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_DEVICE_H_

#import <Foundation/Foundation.h>

@interface PhysicalWebDevice : NSObject

// Initializes a physical web device. See below for the description of the
// properties.
- (instancetype)initWithURL:(NSURL*)url
                 requestURL:(NSURL*)requestURL
                       icon:(NSURL*)icon
                      title:(NSString*)title
                description:(NSString*)description
              transmitPower:(int)transmitPower
                       rssi:(int)rssi
                       rank:(double)rank
              scanTimestamp:(NSDate*)scanTimestamp NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// |url| is the expanded URL.
@property(nonatomic, readonly, strong) NSURL* url;

// |requestUrl| is the broadcast URL.
@property(nonatomic, readonly, strong) NSURL* requestURL;

// |icon| is the URL of the favicon.
@property(nonatomic, readonly, strong) NSURL* icon;

// |title| is the title of the webpage.
@property(nonatomic, readonly, strong) NSString* title;

// |description| is a short text description of the webpage content.
@property(nonatomic, readonly, strong) NSString* description;

// |transmitPower| is the UriBeacon Tx Power Level.
@property(nonatomic, readonly) int transmitPower;

// |rssi| is the received signal strength indicator (RSSI) of the peripheral, in
// decibels.
@property(nonatomic, readonly) int rssi;

// |rank| of the physical web device returned by the server.
@property(nonatomic, readonly) double rank;

// |scanTimestamp| is the time the URL was most recently seen.
@property(nonatomic, strong) NSDate* scanTimestamp;

@end

#endif  // IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_DEVICE_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_TYPES_H_
#define IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_TYPES_H_

#import <Foundation/Foundation.h>

@class PhysicalWebDevice;

namespace physical_web {

typedef void (^RequestFinishedBlock)(PhysicalWebDevice*, NSError*);

// Error domain for physical web.
extern NSString* const kPhysicalWebErrorDomain;

// Error codes for physical web errors.
enum PhysicalWebErrorCode {
  // The JSON data returned by the metadata service is invalid.
  ERROR_INVALID_JSON = 1,
};

// Maximum value for the rank of a physical web device.
extern const double kMaxRank;

}  // namespace physical_web

#endif  // IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_TYPES_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_ALLOC_WITH_ZONE_INTERCEPTOR_H_
#define IOS_WEB_ALLOC_WITH_ZONE_INTERCEPTOR_H_

#import <Foundation/Foundation.h>

namespace web {
// Adds |impl_block| as a custom implementation for |allocWithZone:| method of
// |target| Class. AddAllocWithZoneMethod can not swizzle existing
// |allocWithZone:| method of |target| Class. Neither |target| nor any of its
// superclases must not have overridden |allocWithZone:|.
void AddAllocWithZoneMethod(Class target, id (^impl_block)(Class, NSZone*));
}  // namespace web

#endif  // IOS_WEB_ALLOC_WITH_ZONE_INTERCEPTOR_H_

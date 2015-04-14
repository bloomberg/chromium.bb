// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/alloc_with_zone_interceptor.h"

#import <objc/runtime.h>

#include "base/logging.h"

namespace web {

void AddAllocWithZoneMethod(Class target, id (^impl_block)(Class, NSZone*)) {
  // Make sure |allocWithZone:| is not already implemented in the target class.
  Class meta_class = object_getClass(target);
  DCHECK_EQ(
      class_getMethodImplementation(meta_class, @selector(allocWithZone:)),
      class_getMethodImplementation(object_getClass([NSObject class]),
                                    @selector(allocWithZone:)));

  IMP new_impl = imp_implementationWithBlock(^(id self, NSZone* zone) {
    return impl_block(self, zone);
  });
  class_addMethod(meta_class, @selector(allocWithZone:), new_impl, "v@:@");
}

}  // namespace web

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/mac/objc_method_swizzle.h"

#import "base/logging.h"

namespace ObjcEvilDoers {

Method GetImplementedInstanceMethod(Class aClass, SEL aSelector) {
  Method method = NULL;
  unsigned int methodCount = 0;
  Method* methodList = class_copyMethodList(aClass, &methodCount);
  if (methodList) {
    for (unsigned int i = 0; i < methodCount; ++i) {
      if (method_getName(methodList[i]) == aSelector) {
        method = methodList[i];
        break;
      }
    }
    free(methodList);
  }
  return method;
}

IMP SwizzleImplementedInstanceMethods(
    Class aClass, const SEL originalSelector, const SEL alternateSelector) {
  // The methods must both be implemented by the target class, not
  // inherited from a superclass.
  Method original = GetImplementedInstanceMethod(aClass, originalSelector);
  Method alternate = GetImplementedInstanceMethod(aClass, alternateSelector);
  DCHECK(original);
  DCHECK(alternate);
  if (!original || !alternate) {
    return NULL;
  }

  // The argument and return types must match exactly.
  const char* originalTypes = method_getTypeEncoding(original);
  const char* alternateTypes = method_getTypeEncoding(alternate);
  DCHECK(originalTypes);
  DCHECK(alternateTypes);
  DCHECK(0 == strcmp(originalTypes, alternateTypes));
  if (!originalTypes || !alternateTypes ||
      strcmp(originalTypes, alternateTypes)) {
    return NULL;
  }

  IMP ret = method_getImplementation(original);
  if (ret) {
    method_exchangeImplementations(original, alternate);
  }
  return ret;
}

}  // namespace ObjcEvilDoers

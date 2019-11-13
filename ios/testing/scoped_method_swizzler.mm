// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/scoped_method_swizzler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ScopedMethodSwizzler::ScopedMethodSwizzler(Class klass,
                                           SEL selector_to_replace,
                                           SEL replacing_selector) {
  original_method_ = class_getInstanceMethod(klass, selector_to_replace);
  replacing_method_ = class_getInstanceMethod(klass, replacing_selector);
  BOOL method_added = class_addMethod(
      klass, selector_to_replace, method_getImplementation(replacing_method_),
      method_getTypeEncoding(replacing_method_));
  if (method_added) {
    class_replaceMethod(klass, replacing_selector,
                        method_getImplementation(original_method_),
                        method_getTypeEncoding(original_method_));
  } else {
    method_exchangeImplementations(original_method_, replacing_method_);
  }
}

ScopedMethodSwizzler::~ScopedMethodSwizzler() {
  method_exchangeImplementations(original_method_, replacing_method_);
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_OBJC_METHOD_SWIZZLE_H_
#define CHROME_COMMON_MAC_OBJC_METHOD_SWIZZLE_H_

#import <objc/runtime.h>

// You should think twice every single time you use anything from this
// namespace.
namespace ObjcEvilDoers {

// This is similar to class_getInstanceMethod(), except that it
// returns NULL if |aClass| does not directly implement |aSelector|.
Method GetImplementedInstanceMethod(Class aClass, SEL aSelector);

// Exchanges the implementation of |originalSelector| and
// |alternateSelector| within |aClass|.  Both selectors must be
// implemented directly by |aClass|, not inherited.  The IMP returned
// is for |originalSelector| (for purposes of forwarding).
IMP SwizzleImplementedInstanceMethods(
    Class aClass, const SEL originalSelector, const SEL alternateSelector);

}  // namespace ObjcEvilDoers

#endif  // CHROME_COMMON_MAC_OBJC_METHOD_SWIZZLE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TYPE_ID_TEST_SUPPORT_B_H_
#define BASE_TEST_TYPE_ID_TEST_SUPPORT_B_H_

#include "base/type_id.h"

namespace base {

// BASE_EXPORT depends on COMPONENT_BUILD.
// This will always be a separate shared library, so don't use BASE_EXPORT here.
#if defined(WIN32)
#define TEST_SUPPORT_EXPORT __declspec(dllexport)
#else
#define TEST_SUPPORT_EXPORT __attribute__((visibility("default")))
#endif  // defined(WIN32)

// This is here to help test base::TypeId.
struct TEST_SUPPORT_EXPORT TypeIdTestSupportB {
  static TypeId GetTypeIdForTypeInAnonymousNameSpace();
  static TypeId GetTypeIdForUniquePtrInt();
};

}  // namespace base

#endif  // BASE_TEST_TYPE_ID_TEST_SUPPORT_B_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_TEST_GTEST_H_
#define GIN_TEST_GTEST_H_

#include "v8/include/v8.h"

namespace gin {

v8::Local<v8::ObjectTemplate> GetGTestTemplate(v8::Isolate* isolate);

}  // namespace gin

#endif  // GIN_TEST_GTEST_H_

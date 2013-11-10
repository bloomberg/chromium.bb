// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_WRAPPER_INFO_H_
#define GIN_WRAPPER_INFO_H_

#include "v8/include/v8.h"

namespace gin {

enum InternalFields {
  kWrapperInfoIndex,
  kEncodedValueIndex,
  kNumberOfInternalFields,
};

struct WrapperInfo {
  static WrapperInfo* From(v8::Handle<v8::Object> object);
  // Currently we just use the address of this struct for identity.
};

}  // namespace gin

#endif  // GIN_WRAPPER_INFO_H_

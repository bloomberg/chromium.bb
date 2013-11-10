// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/wrapper_info.h"

namespace gin {

WrapperInfo* WrapperInfo::From(v8::Handle<v8::Object> object) {
  if (object->InternalFieldCount() != kNumberOfInternalFields)
    return NULL;
  return static_cast<WrapperInfo*>(
      object->GetAlignedPointerFromInternalField(kWrapperInfoIndex));
}

}  // namespace gin

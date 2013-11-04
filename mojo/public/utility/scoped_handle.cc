// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/utility/scoped_handle.h"

namespace mojo {

ScopedHandle::ScopedHandle(Handle handle) : handle_(handle) {
}

ScopedHandle::~ScopedHandle() {
  if (handle_.value == kInvalidHandle.value)
    return;
  Close(handle_);
  Release();
}

Handle ScopedHandle::Release() {
  Handle temp = handle_;
  handle_ = kInvalidHandle;
  return temp;
}

}  // namespace mojo

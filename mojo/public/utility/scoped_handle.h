// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_UTILITY_SCOPED_HANDLE_H_
#define MOJO_PUBLIC_UTILITY_SCOPED_HANDLE_H_

#include "mojo/public/system/core.h"

namespace mojo {

// Closes handle_ when deleted.
// This probably wants tons of improvements, but those can be made as needed.
class ScopedHandle {
 public:
  ScopedHandle(Handle handle);
  ~ScopedHandle();
  Handle get() const { return handle_; }
  Handle Release();

 private:
  Handle handle_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SYSTEM_SCOPED_HANDLE_H_

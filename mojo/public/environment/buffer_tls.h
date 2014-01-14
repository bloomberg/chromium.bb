// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_ENVIRONMENT_BUFFER_TLS_H_
#define MOJO_PUBLIC_ENVIRONMENT_BUFFER_TLS_H_

namespace mojo {
class Buffer;

namespace internal {

// Get/Set the |Buffer*| associated with current thread.
Buffer* GetCurrentBuffer();
Buffer* SetCurrentBuffer(Buffer* buffer);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_ENVIRONMENT_BUFFER_TLS_H_

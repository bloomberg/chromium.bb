// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_EMBEDDER_INTERNAL_H_
#define MOJO_EDK_EMBEDDER_EMBEDDER_INTERNAL_H_

namespace mojo {

namespace system {
class Core;
}  // namespace system

namespace embedder {
namespace internal {

// Instance of |Core| used by the system functions (|Mojo...()|).
extern system::Core* g_core;

}  // namespace internal
}  // namepace embedder

}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_EMBEDDER_INTERNAL_H_

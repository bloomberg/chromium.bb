// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_DEBUG_H_
#define GIN_PUBLIC_DEBUG_H_

#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

class GIN_EXPORT Debug {
 public:
  /* Installs a callback that is invoked on entry to every V8-generated
   * function.
   *
   * This only affects IsolateHolder instances created after
   * SetFunctionEntryHook was invoked.
   */
  static void SetFunctionEntryHook(v8::FunctionEntryHook entry_hook);

  /* Installs a callback that is invoked each time jit code is added, moved,
   * or removed.
   *
   * This only affects IsolateHolder instances created after
   * SetJitCodeEventHandler was invoked.
   */
  static void SetJitCodeEventHandler(v8::JitCodeEventHandler event_handler);
};

}  // namespace gin

#endif  // GIN_PUBLIC_DEBUG_H_

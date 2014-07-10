// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_VIRTUAL_KEYBOARD_PUBLIC_VIRTUAL_KEYBOARD_BINDINGS_H_
#define ATHENA_VIRTUAL_KEYBOARD_PUBLIC_VIRTUAL_KEYBOARD_BINDINGS_H_

#include "athena/athena_export.h"

namespace content {
class RenderView;
}

namespace athena {

// This provides the necessary bindings for the virtualKeyboardPrivate API.
class ATHENA_EXPORT VirtualKeyboardBindings {
 public:
  // Creates the bindings for |render_view|.
  static VirtualKeyboardBindings* Create(content::RenderView* render_view);
};

}  // namespace athena

#endif  // ATHENA_VIRTUAL_KEYBOARD_PUBLIC_VIRTUAL_KEYBOARD_BINDINGS_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_

#include "content/common/content_export.h"

namespace content {

struct NativeWebKeyboardEvent;

class CONTENT_EXPORT KeyboardListener {
 public:
  virtual bool HandleKeyPressEvent(const NativeWebKeyboardEvent& event) = 0;

 protected:
  virtual ~KeyboardListener() {}
};

}  // content

#endif  // CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_

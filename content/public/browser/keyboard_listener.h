// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_

#include "content/common/content_export.h"

// All systems should support key listening through ui::KeyEvent.
// GTK doesn't because there is no easy way to convet GdkEventKey to
// ui::KeyEvent and since it is going away soon, it is not worth the effort.
#if defined(TOOLKIT_GTK)
typedef struct _GdkEventKey GdkEventKey;
#else
namespace ui {
class KeyEvent;
}  // namespace ui
#endif

class CONTENT_EXPORT KeyboardListener {
 public:
#if defined(TOOLKIT_GTK)
  virtual bool HandleKeyPressEvent(GdkEventKey* event) = 0;
#else
  virtual bool HandleKeyPressEvent(ui::KeyEvent* event) = 0;
#endif

 protected:
  virtual ~KeyboardListener() {}
};

#endif  // CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_

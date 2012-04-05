// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_
#pragma once

#include "content/common/content_export.h"

#if defined(TOOLKIT_GTK)
typedef struct _GdkEventKey GdkEventKey;
#endif  // defined(TOOLKIT_GTK)

class CONTENT_EXPORT KeyboardListener {
 public:
#if defined(TOOLKIT_GTK)
  virtual bool HandleKeyPressEvent(GdkEventKey* event) = 0;
#endif  // defined(TOOLKIT_GTK)

 protected:
  virtual ~KeyboardListener() {}
};

#endif  // CONTENT_PUBLIC_BROWSER_KEYBOARD_LISTENER_H_

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_GLX_H_
#define CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_GLX_H_

#include <GL/glx.h>
#include <X11/Xlib.h>

#include "base/basictypes.h"

class BackingStoreManagerGlx {
 public:
  BackingStoreManagerGlx();
  ~BackingStoreManagerGlx();

  Display* display() const { return display_; }

  // Returns the context, creating it if necessary, and binding it to the given
  // display and window identified by the XID. This will avoid duplicate calls
  // to MakeCurrent if the display/XID hasn't changed from the last call.
  // Returns NULL on failure.
  GLXContext BindContext(XID window_id);

 private:
  Display* display_;

  // Set to true when we've tried to create the context. This prevents us from
  // trying to initialize the OpenGL context over and over in the failure case.
  bool tried_to_init_;

  // The OpenGL context. Non-NULL when initialized.
  GLXContext context_;

  // The last window we've bound our context to. This allows us to avoid
  // duplicate "MakeCurrent" calls which are expensive.
  XID previous_window_id_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreManagerGlx);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_GLX_H_

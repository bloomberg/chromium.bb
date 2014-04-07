// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_HANDLER_H_

// Bindings implementations must subclass this. Used so that MojoWebUIController
// can own the binding implementation.
class MojoWebUIHandler {
 public:
  MojoWebUIHandler() {}
  virtual ~MojoWebUIHandler() {}
};

#endif  // CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_HANDLER_H_

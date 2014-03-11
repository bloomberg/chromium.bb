// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTO_KEEP_ALIVE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTO_KEEP_ALIVE_H_

#include "ui/gfx/native_widget_types.h"

// Class to scoped decrement keep alive count.
class AutoKeepAlive {
 public:
  explicit AutoKeepAlive(gfx::NativeWindow window);
  ~AutoKeepAlive();

 private:
  bool keep_alive_available_;

  DISALLOW_COPY_AND_ASSIGN(AutoKeepAlive);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTO_KEEP_ALIVE_H_

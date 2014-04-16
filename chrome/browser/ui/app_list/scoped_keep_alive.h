// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SCOPED_KEEP_ALIVE_H_
#define CHROME_BROWSER_UI_APP_LIST_SCOPED_KEEP_ALIVE_H_

#include "base/macros.h"

// A class that can be put in a scoped_ptr to represent a "keep alive" resource.
class ScopedKeepAlive {
 public:
  explicit ScopedKeepAlive();
  ~ScopedKeepAlive();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedKeepAlive);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_SCOPED_KEEP_ALIVE_H_

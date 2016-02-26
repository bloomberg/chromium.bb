// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SCOPED_KEEP_ALIVE_H_
#define CHROME_BROWSER_LIFETIME_SCOPED_KEEP_ALIVE_H_

#include "base/macros.h"

enum class KeepAliveOrigin;

// Registers with KeepAliveRegistry on creation and unregisters them
// on destruction. Use these objects with a scoped_ptr for easy management.
class ScopedKeepAlive {
 public:
  explicit ScopedKeepAlive(KeepAliveOrigin origin);
  ~ScopedKeepAlive();

 private:
  const KeepAliveOrigin origin_;

  DISALLOW_COPY_AND_ASSIGN(ScopedKeepAlive);
};

#endif  // CHROME_BROWSER_LIFETIME_SCOPED_KEEP_ALIVE_H_

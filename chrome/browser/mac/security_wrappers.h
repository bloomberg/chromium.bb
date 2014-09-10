// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_SECURITY_WRAPPERS_H_
#define CHROME_BROWSER_MAC_SECURITY_WRAPPERS_H_

#include <CoreFoundation/CFBase.h>

#include "base/macros.h"

namespace chrome {

// Wraps SecKeychainSetUserInteractionAllowed, restoring the previous setting
// on destruction.
class ScopedSecKeychainSetUserInteractionAllowed {
 public:
  explicit ScopedSecKeychainSetUserInteractionAllowed(Boolean allowed);
  ~ScopedSecKeychainSetUserInteractionAllowed();

 private:
  Boolean old_allowed_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSecKeychainSetUserInteractionAllowed);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MAC_SECURITY_WRAPPERS_H_

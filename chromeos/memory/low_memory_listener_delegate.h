// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_LOW_MEMORY_LISTENER_DELEGATE_H_
#define CHROMEOS_MEMORY_LOW_MEMORY_LISTENER_DELEGATE_H_

#include "chromeos/memory/chromeos_memory_export.h"

namespace chromeos {

class CHROMEOS_MEMORY_EXPORT LowMemoryListenerDelegate {
 public:
  // Invoked when a low memory situation is detected.
  virtual void OnMemoryLow() = 0;

 protected:
  virtual ~LowMemoryListenerDelegate() {}
};

}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_LOW_MEMORY_LISTENER_DELEGATE_H_

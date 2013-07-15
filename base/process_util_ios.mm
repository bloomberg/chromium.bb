// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#import <Foundation/Foundation.h>
#include <stdio.h>
#include <sys/resource.h>

#include "base/logging.h"

// This is just enough of a shim to let the support needed by test_support
// link.  In general, process_util isn't valid on iOS.

namespace base {

void RaiseProcessToHighPriority() {
  // Impossible on iOS. Do nothing.
}

}  // namespace base

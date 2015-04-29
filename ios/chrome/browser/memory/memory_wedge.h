// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MEMORY_MEMORY_WEDGE_H_
#define IOS_CHROME_BROWSER_MEMORY_MEMORY_WEDGE_H_

#include <stdlib.h>

namespace memory_wedge {

// Allocates a chunk of memory that will stay until termination.
// If wedge_size_in_mb is 0, this is a no-op.
void AddWedge(size_t wedge_size_in_mb);

// For testing only.
void RemoveWedgeForTesting();

}  // namespace memory_wedge

#endif  // IOS_CHROME_BROWSER_MEMORY_MEMORY_WEDGE_H_

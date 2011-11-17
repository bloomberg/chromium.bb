// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GAMEPAD_HARDWARE_BUFFER_H_
#define CONTENT_COMMON_GAMEPAD_HARDWARE_BUFFER_H_

#include "base/atomicops.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGamepads.h"

namespace gamepad {

/*

This structure is stored in shared memory that's shared between the browser
which does the hardware polling, and the various consumers of the gamepad
state (renderers and NaCl plugins). The performance characteristics are that
we want low latency (so would like to avoid explicit communication via IPC
between producer and consumer) and relatively large data size.

Writer and reader operate on the same buffer assuming contention is low, and
start_marker and end_marker are used to detect inconsistent reads.

The writer atomically increments the start_marker counter before starting,
then fills in the gamepad data, then increments end_marker to the same value
as start_marker. The readers first reads end_marker, then the the data and
then start_marker, and if the reader finds that the start and end markers were
different, then it must retry as the buffer was updated while being read.

There is a requirement for memory barriers between the accesses to the markers
and the main data to ensure that both the reader and write see a consistent
view of those values. In the current implementation, the writer uses an
Barrier_AtomicIncrement for the counter, and the reader uses an Acquire_Load.

*/

struct GamepadHardwareBuffer {
  base::subtle::Atomic32 start_marker;
  WebKit::WebGamepads buffer;
  base::subtle::Atomic32 end_marker;
};

}

#endif // CONTENT_COMMON_GAMEPAD_HARDWARE_BUFFER_H_

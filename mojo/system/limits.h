// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_LIMITS_H_
#define MOJO_SYSTEM_LIMITS_H_

namespace mojo {
namespace system {

// Maximum of open (Mojo) handles.
// TODO(vtl): This doesn't count "live" handles, some of which may live in
// messages.
const size_t kMaxHandleTableSize = 1000000;

const size_t kMaxWaitManyNumHandles = kMaxHandleTableSize;

const size_t kMaxMessageNumBytes = 4 * 1024 * 1024;

const size_t kMaxMessageNumHandles = 10000;

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_LIMITS_H_

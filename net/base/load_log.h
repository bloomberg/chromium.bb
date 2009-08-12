// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_LOG_H_
#define NET_BASE_LOAD_LOG_H_

#include "base/ref_counted.h"

namespace net {

// LoadLog stores profiling information on where time was spent while servicing
// a request (waiting in queues, resolving hosts, resolving proxy, etc...).
class LoadLog : public base::RefCounted<LoadLog> {
 public:
// TODO(eroman): Add an API similar to:
//     void TrackEnterState(LoadState state);
//     void TrackLeaveState(LoadState state);
//     void Merge(const LoadLog* other);
};

}  // namespace net

#endif  // NET_BASE_LOAD_LOG_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_REQUEST_PRIORITY_H_
#define NET_BASE_REQUEST_PRIORITY_H_

namespace net {

// Prioritization used in various parts of the networking code such
// as connection prioritization and resource loading prioritization.
enum RequestPriority {
  IDLE = 0,
  MINIMUM_PRIORITY = IDLE,
  LOWEST,
  DEFAULT_PRIORITY = LOWEST,
  LOW,
  MEDIUM,
  HIGHEST,
  NUM_PRIORITIES,
};

const char* RequestPriorityToString(RequestPriority priority);

}  // namespace net

#endif  // NET_BASE_REQUEST_PRIORITY_H_

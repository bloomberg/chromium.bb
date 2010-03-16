// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "chrome/browser/sync/engine/idle_query_linux.h"

IdleState CalculateIdleState(unsigned int idle_threshold) {
  browser_sync::IdleQueryLinux idle_query;
  unsigned int idle_time = idle_query.IdleTime();
  if (idle_time >= idle_threshold)
    return IDLE_STATE_IDLE;
  return IDLE_STATE_ACTIVE;
}

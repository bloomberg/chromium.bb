// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_STATE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_STATE_H_

namespace resource_coordinator {

enum class LifecycleState {
  // The LifecycleUnit is alive and active.
  ACTIVE,
  // The LifecycleUnit is pending a freeze.
  PENDING_FREEZE,
  // The LifecycleUnit is frozen.
  FROZEN,
  // The LifecycleUnit is pending a discard.
  PENDING_DISCARD,
  // The LifecycleUnit is discarded, and is consuming no system resources.
  DISCARDED,
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_STATE_H_

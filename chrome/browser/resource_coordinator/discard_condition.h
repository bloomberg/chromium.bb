// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_CONDITION_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_CONDITION_H_

namespace resource_coordinator {

enum class DiscardCondition {
  // The discard happens proactively when the machine is in a good state.
  kProactive,
  // The discard happens because the machine is in a critical condition.
  kUrgent,
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_CONDITION_H_

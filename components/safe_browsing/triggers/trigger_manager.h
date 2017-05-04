// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_

#include "base/macros.h"

namespace safe_browsing {

// This class manager SafeBrowsing triggers. It has two main responsibilities:
// 1) ensuring triggers only run when appropriate by honouring user opt-ins and
// incognito state, and 2) tracking how often triggers fire and throttling them
// when necessary.
class TriggerManager {
 public:
  TriggerManager();
  ~TriggerManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(TriggerManager);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_

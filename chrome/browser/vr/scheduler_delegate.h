// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_
#define CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

class VR_EXPORT SchedulerDelegate {
 public:
  using DrawCallback = base::RepeatingCallback<void(base::TimeTicks)>;
  virtual ~SchedulerDelegate() {}

  // TODO(acondor): Drop "Scheduler" from these names once RenderLoop owns
  // VrShellGl.
  virtual void OnSchedulerPause() = 0;
  virtual void OnSchedulerResume() = 0;

  virtual void SetDrawWebXrCallback(DrawCallback callback) = 0;
  virtual void SetDrawBrowserCallback(DrawCallback callback) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_

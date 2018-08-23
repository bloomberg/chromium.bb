// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_
#define CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace gfx {
class Transform;
}

namespace vr {

class VR_EXPORT SchedulerDelegate {
 public:
  using DrawCallback = base::RepeatingCallback<void(base::TimeTicks)>;
  using WebXrInputCallback =
      base::RepeatingCallback<void(const gfx::Transform&, base::TimeTicks)>;
  virtual ~SchedulerDelegate() {}

  virtual void OnPause() = 0;
  virtual void OnResume() = 0;

  virtual void OnExitPresent() = 0;
  virtual void OnTriggerEvent(bool pressed) = 0;
  virtual void SetWebXrMode(bool enabled) = 0;
  virtual void SetDrawWebXrCallback(DrawCallback callback) = 0;
  virtual void SetDrawBrowserCallback(DrawCallback callback) = 0;
  virtual void SetWebXrInputCallback(WebXrInputCallback callback) = 0;
  virtual void AddInputSourceState(
      device::mojom::XRInputSourceStatePtr state) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_

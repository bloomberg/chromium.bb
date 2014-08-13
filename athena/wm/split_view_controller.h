// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_
#define ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_

#include "athena/wm/bezel_controller.h"
#include "base/memory/scoped_ptr.h"

namespace athena {

// Responsible for entering split view mode, exiting from split view mode, and
// laying out the windows in split view mode.
class SplitViewController : public BezelController::ScrollDelegate {
 public:
  SplitViewController();
  virtual ~SplitViewController();

 private:
  // BezelController::ScrollDelegate overrides.
  virtual void ScrollBegin(BezelController::Bezel bezel, float delta) OVERRIDE;
  virtual void ScrollEnd() OVERRIDE;
  virtual void ScrollUpdate(float delta) OVERRIDE;
  virtual bool CanScroll() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SplitViewController);
};

}  // namespace athena

#endif  // ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_

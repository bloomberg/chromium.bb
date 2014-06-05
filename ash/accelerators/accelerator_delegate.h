// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_DELEGATE_H_
#define ASH_ACCELERATORS_ACCELERATOR_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/wm/core/accelerator_delegate.h"

namespace ash {

class ASH_EXPORT AcceleratorDelegate
    : NON_EXPORTED_BASE(public wm::AcceleratorDelegate) {
 public:
  AcceleratorDelegate();
  virtual ~AcceleratorDelegate();

  // wm::AcceleratorDelegate:
  virtual bool ProcessAccelerator(const ui::KeyEvent& event,
                                  const ui::Accelerator& accelerator,
                                  KeyType key_type) OVERRIDE;

 private:
  // Returns true if the window should be allowed a chance to handle
  // system keys.
  bool CanConsumeSystemKeys(const ui::KeyEvent& event);

  bool ShouldProcessAcceleratorNow(const ui::KeyEvent& event,
                                   const ui::Accelerator& accelerator);

  DISALLOW_COPY_AND_ASSIGN(AcceleratorDelegate);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_DELEGATE_H_

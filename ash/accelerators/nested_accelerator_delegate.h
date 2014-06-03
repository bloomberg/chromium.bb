// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_NESTED_ACCELERATOR_DELEGATE_H_
#define ASH_ACCELERATORS_NESTED_ACCELERATOR_DELEGATE_H_

#include "base/macros.h"
#include "ui/wm/core/nested_accelerator_delegate.h"

namespace ash {

class NestedAcceleratorDelegate : public wm::NestedAcceleratorDelegate {
 public:
  NestedAcceleratorDelegate();
  virtual ~NestedAcceleratorDelegate();

  // wm::AcceleratorDispatcher::Delegate
  virtual Result ProcessAccelerator(
      const ui::Accelerator& accelerator) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NestedAcceleratorDelegate);
};

}  // namespace

#endif  // ASH_ACCELERATORS_NESTED_ACCELERATOR_DELEGATE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_VOLUME_CONTROL_DELEGATE_H_
#define ASH_TEST_TEST_VOLUME_CONTROL_DELEGATE_H_

#include "ash/volume_control_delegate.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

// A simple test double for a VolumeControlDelegate
// Will count the number of times the HandleVolumeMute, HandleVolumeDown and
// HandleVolumeUp methods are invoked.
class TestVolumeControlDelegate : public ash::VolumeControlDelegate {
 public:
  explicit TestVolumeControlDelegate(bool consume);
  virtual ~TestVolumeControlDelegate();

  int handle_volume_mute_count() const {
    return handle_volume_mute_count_;
  }

  int handle_volume_down_count() const {
    return handle_volume_down_count_;
  }

  int handle_volume_up_count() const {
    return handle_volume_up_count_;
  }

  const ui::Accelerator& last_accelerator() const {
    return last_accelerator_;
  }

  // ash::VolumeControlDelegate:
  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) OVERRIDE;

 private:
  // Keeps track of the return value that should be used for the methods
  // inherited from VolumeControlDelegate
  bool consume_;
  int handle_volume_mute_count_;
  int handle_volume_down_count_;
  int handle_volume_up_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(TestVolumeControlDelegate);
};

}  // namespace ash

#endif  // ASH_TEST_TEST_VOLUME_CONTROL_DELEGATE_H_

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
  TestVolumeControlDelegate();
  ~TestVolumeControlDelegate() override;

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
  void HandleVolumeMute(const ui::Accelerator& accelerator) override;
  void HandleVolumeDown(const ui::Accelerator& accelerator) override;
  void HandleVolumeUp(const ui::Accelerator& accelerator) override;

 private:
  int handle_volume_mute_count_;
  int handle_volume_down_count_;
  int handle_volume_up_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(TestVolumeControlDelegate);
};

}  // namespace ash

#endif  // ASH_TEST_TEST_VOLUME_CONTROL_DELEGATE_H_

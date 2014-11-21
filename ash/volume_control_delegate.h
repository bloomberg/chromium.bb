// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VOLUME_CONTROL_DELEGATE_H_
#define ASH_VOLUME_CONTROL_DELEGATE_H_

namespace ui {
class Accelerator;
}  // namespace ui

namespace ash {

// Delegate for controlling the volume.
class VolumeControlDelegate {
 public:
  virtual ~VolumeControlDelegate() {}

  virtual void HandleVolumeMute(const ui::Accelerator& accelerator) = 0;
  virtual void HandleVolumeDown(const ui::Accelerator& accelerator) = 0;
  virtual void HandleVolumeUp(const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_VOLUME_CONTROL_DELEGATE_H_

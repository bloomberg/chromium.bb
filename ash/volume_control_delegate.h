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

  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) = 0;
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) = 0;
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_VOLUME_CONTROL_DELEGATE_H_

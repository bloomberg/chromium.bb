// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_VOLUME_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_VOLUME_CONTROLLER_CHROMEOS_H_

#include "ash/public/interfaces/volume.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

// Controls the volume when F8-10 or a multimedia key for volume is pressed.
class VolumeController : public ash::mojom::VolumeController {
 public:
  VolumeController();
  ~VolumeController() override;

  // Binds the mojom::VolumeController interface request to this object.
  void BindRequest(ash::mojom::VolumeControllerRequest request);

  // Overridden from ash::mojom::VolumeController:
  void VolumeMute() override;
  void VolumeDown() override;
  void VolumeUp() override;

 private:
  mojo::BindingSet<ash::mojom::VolumeController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(VolumeController);
};

#endif  // CHROME_BROWSER_UI_ASH_VOLUME_CONTROLLER_CHROMEOS_H_

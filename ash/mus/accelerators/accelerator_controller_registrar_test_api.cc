// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/accelerators/accelerator_controller_registrar_test_api.h"

#include "ash/mus/accelerators/accelerator_controller_registrar.h"
#include "ash/mus/accelerators/accelerator_ids.h"
#include "ash/mus/bridge/wm_shell_mus_test_api.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"

namespace ash {
namespace mus {

AcceleratorControllerRegistrarTestApi::AcceleratorControllerRegistrarTestApi() {
}

AcceleratorControllerRegistrarTestApi::
    ~AcceleratorControllerRegistrarTestApi() {}

void AcceleratorControllerRegistrarTestApi::ProcessAccelerator(
    const ui::Accelerator& accelerator) {
  const ui::KeyEvent key_event(accelerator.type(), accelerator.key_code(),
                               accelerator.modifiers());
  AcceleratorControllerRegistrar* registrar =
      WmShellMusTestApi().accelerator_controller_registrar();
  const AcceleratorControllerRegistrar::Ids& ids =
      registrar->accelerator_to_ids_[accelerator];
  const uint32_t pre_id =
      ComputeAcceleratorId(registrar->id_namespace_, ids.pre_id);
  if (registrar->OnAccelerator(pre_id, key_event) ==
      ui::mojom::EventResult::HANDLED) {
    return;
  }

  const uint32_t post_id =
      ComputeAcceleratorId(registrar->id_namespace_, ids.post_id);
  registrar->OnAccelerator(post_id, key_event);
}

}  // namespace mus
}  // namespace ash

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_REGISTRAR_TEST_API_H_
#define ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_REGISTRAR_TEST_API_H_

#include "base/macros.h"

namespace ui {
class Accelerator;
}

namespace ash {
namespace mus {

class AcceleratorControllerRegistrarTestApi {
 public:
  AcceleratorControllerRegistrarTestApi();
  ~AcceleratorControllerRegistrarTestApi();

  // Sends |accelerator| to AcceleratorControllerRegistrar for the pre phase,
  // and if the pre phase does not process the event the post phase as well.
  void ProcessAccelerator(const ui::Accelerator& accelerator);

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerRegistrarTestApi);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_REGISTRAR_TEST_API_H_

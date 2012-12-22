// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller.h"

#if defined(HAVE_IBUS)
#include "chrome/browser/chromeos/input_method/ibus_controller_impl.h"
#else
#include "chrome/browser/chromeos/input_method/mock_ibus_controller.h"
#endif

namespace chromeos {
namespace input_method {

IBusController::~IBusController() {
}

// static
IBusController* IBusController::Create(
    const scoped_refptr<base::SequencedTaskRunner>& default_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner) {
#if defined(HAVE_IBUS)
  return new IBusControllerImpl(default_task_runner, worker_task_runner);
#else
  return new MockIBusController;
#endif
}

}  // namespace input_method
}  // namespace chromeos

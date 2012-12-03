// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"

#include "chrome/browser/chromeos/input_method/candidate_window_controller_impl.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"

namespace chromeos {
namespace input_method {

// static
CandidateWindowController*
CandidateWindowController::CreateCandidateWindowController(
    IBusController* controller) {
  CandidateWindowControllerImpl* candidate_window_controller =
      new CandidateWindowControllerImpl;
  // TODO(nona): Refine observer chain once IBusUiController is removed.
  controller->AddObserver(candidate_window_controller);
  return candidate_window_controller;
}

}  // namespace input_method
}  // namespace chromeos

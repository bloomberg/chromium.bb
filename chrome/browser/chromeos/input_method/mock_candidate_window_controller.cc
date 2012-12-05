// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_candidate_window_controller.h"

namespace chromeos {
namespace input_method {

MockCandidateWindowController::MockCandidateWindowController()
    : init_count_(0),
      add_observer_count_(0),
      remove_observer_count_(0) {
}

MockCandidateWindowController::~MockCandidateWindowController() {
}

bool MockCandidateWindowController::Init(IBusController* controller) {
  ++init_count_;
  return true;
}

void MockCandidateWindowController::Shutdown(IBusController* controller) {
}

void MockCandidateWindowController::AddObserver(
    CandidateWindowController::Observer* observer) {
  ++add_observer_count_;
  observers_.AddObserver(observer);
}

void MockCandidateWindowController::RemoveObserver(
    CandidateWindowController::Observer* observer) {
  ++remove_observer_count_;
  observers_.RemoveObserver(observer);
}

void MockCandidateWindowController::NotifyCandidateWindowOpened() {
  FOR_EACH_OBSERVER(CandidateWindowController::Observer, observers_,
                    CandidateWindowOpened());
}

void MockCandidateWindowController::NotifyCandidateWindowClosed() {
  FOR_EACH_OBSERVER(CandidateWindowController::Observer, observers_,
                    CandidateWindowClosed());
}

}  // namespace input_method
}  // namespace chromeos

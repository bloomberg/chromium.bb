// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"

namespace chromeos {
namespace input_method {

// The mock CandidateWindowController implementation for testing.
class MockCandidateWindowController : public CandidateWindowController {
 public:
  MockCandidateWindowController();
  ~MockCandidateWindowController() override;

  // CandidateWindowController overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Hide() override;

  // Notifies observers.
  void NotifyCandidateWindowOpened();
  void NotifyCandidateWindowClosed();

  int add_observer_count_;
  int remove_observer_count_;
  int hide_count_;

 private:
  base::ObserverList<CandidateWindowController::Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(MockCandidateWindowController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_

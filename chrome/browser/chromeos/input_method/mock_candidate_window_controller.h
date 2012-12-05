// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"

namespace chromeos {
namespace input_method {

// The mock CandidateWindowController implementation for testing.
class MockCandidateWindowController : public CandidateWindowController {
 public:
  MockCandidateWindowController();
  virtual ~MockCandidateWindowController();

  // CandidateWindowController overrides:
  virtual bool Init(IBusController* controller) OVERRIDE;
  virtual void Shutdown(IBusController* controller) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

  // Notifies observers.
  void NotifyCandidateWindowOpened();
  void NotifyCandidateWindowClosed();

  int init_count_;
  int add_observer_count_;
  int remove_observer_count_;

 private:
  ObserverList<CandidateWindowController::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(MockCandidateWindowController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_H_

#include "base/basictypes.h"

namespace chromeos {
namespace input_method {

class IBusController;

// CandidateWindowController is used for controlling the input method
// candidate window. Once the initialization is done, the controller
// starts monitoring signals sent from the the background input method
// daemon, and shows and hides the candidate window as neeeded. Upon
// deletion of the object, monitoring stops and the view used for
// rendering the candidate view is deleted.
class CandidateWindowController {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    virtual void CandidateWindowOpened() = 0;
    virtual void CandidateWindowClosed() = 0;
  };

  virtual ~CandidateWindowController() {}

  // Initializes the candidate window. Returns true on success. |controller| can
  // be NULL.
  // TODO(nona): Refine observer chain once IBusUiController is removed.
  virtual bool Init(IBusController* controller) = 0;

  // Shutdown the candidate window controller. |controller| can be NULL.
  // TODO(nona): Refine observer chain once IBusUiController is removed.
  virtual void Shutdown(IBusController* controller) = 0;
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets an instance of CandidateWindowController. Caller has to delete the
  // returned object.
  static CandidateWindowController* CreateCandidateWindowController();
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_H_

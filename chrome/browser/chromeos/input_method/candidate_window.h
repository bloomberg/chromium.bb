// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_H_
#pragma once

#include "base/basictypes.h"

namespace chromeos {

// CandidateWindowController is used for controlling the input method
// candidate window. Once the initialization is done, the controller
// starts monitoring signals sent from the the background input method
// daemon, and shows and hides the candidate window as neeeded. Upon
// deletion of the object, monitoring stops and the view used for
// rendering the candidate view is deleted.
class CandidateWindowController {
 public:
  CandidateWindowController();
  virtual ~CandidateWindowController();

  // Initializes the candidate window. Returns true on success.
  bool Init();

 private:
  class Impl;
  Impl* impl_;
  DISALLOW_COPY_AND_ASSIGN(CandidateWindowController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_H_

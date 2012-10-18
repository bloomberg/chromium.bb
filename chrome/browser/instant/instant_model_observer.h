// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_MODEL_OBSERVER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_MODEL_OBSERVER_H_

#include "chrome/common/instant_types.h"

class InstantModel;

// This class defines the observer interface for the |InstantModel|.
class InstantModelObserver {
 public:
  // Informs the observer that the preview state has has changed.
  // This can mean the model state has changed, or the contents of the
  // preview.
  virtual void DisplayStateChanged(const InstantModel& model) = 0;

 protected:
  virtual ~InstantModelObserver() {}
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_MODEL_OBSERVER_H_

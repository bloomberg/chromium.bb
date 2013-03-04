// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_MODEL_OBSERVER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_MODEL_OBSERVER_H_

class InstantOverlayModel;

// This class defines the observer interface for the InstantOverlayModel.
class InstantOverlayModelObserver {
 public:
  // Informs the observer that the overlay state has changed. This can mean
  // either the model state changed or the contents of the overlay changed.
  virtual void OverlayStateChanged(const InstantOverlayModel& model) = 0;

 protected:
  ~InstantOverlayModelObserver() {}
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_MODEL_OBSERVER_H_

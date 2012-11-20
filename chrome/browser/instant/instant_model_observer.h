// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_MODEL_OBSERVER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_MODEL_OBSERVER_H_

class InstantModel;

// This class defines the observer interface for the |InstantModel|.
class InstantModelObserver {
 public:
  // Informs the observer that the preview state has changed. This can mean
  // either the model state changed or the contents of the preview changed.
  virtual void PreviewStateChanged(const InstantModel& model) = 0;

 protected:
  ~InstantModelObserver() {}
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_MODEL_OBSERVER_H_

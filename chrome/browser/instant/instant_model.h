// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_MODEL_H_
#define CHROME_BROWSER_INSTANT_INSTANT_MODEL_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/search_types.h"

class InstantController;
class InstantModelObserver;

namespace content {
class WebContents;
}

// Holds state that is important to any views concerned with visibility and
// layout of the Instant preview.
class InstantModel {
 public:
  explicit InstantModel(InstantController* controller);
  ~InstantModel();

  // InstantModel only uses Mode::mode internally. Other parts of Mode, such as
  // Mode::origin, may have arbitrary values, and should be ignored.
  const chrome::search::Mode& mode() const { return mode_; }
  int height() const { return height_; }
  InstantSizeUnits height_units() const { return height_units_; }

  void SetPreviewState(const chrome::search::Mode& mode,
                       int height,
                       InstantSizeUnits height_units);

  void SetPreviewContents(content::WebContents* preview_contents);
  content::WebContents* GetPreviewContents() const;

  // Add and remove observers.
  void AddObserver(InstantModelObserver* observer) const;
  void RemoveObserver(InstantModelObserver* observer) const;

 private:
  chrome::search::Mode mode_;
  int height_;
  InstantSizeUnits height_units_;

  // Weak. Remembers the last set preview contents to detect changes. Actual
  // preview contents is fetched from the |controller_| as this may not always
  // reflect the actual preview in effect.
  content::WebContents* preview_contents_;

  // Weak. The controller currently holds some model state.
  // TODO(dhollowa): Remove this, transfer all model state to InstantModel.
  InstantController* const controller_;

  // Observers.
  mutable ObserverList<InstantModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(InstantModel);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_MODEL_H_

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_MODEL_H_
#define CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_MODEL_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/search_types.h"

class InstantController;
class InstantOverlayModelObserver;

namespace content {
class WebContents;
}

// Holds state that is important to any views concerned with visibility and
// layout of the Instant overlay.
class InstantOverlayModel {
 public:
  explicit InstantOverlayModel(InstantController* controller);
  ~InstantOverlayModel();

  // InstantOverlayModel only uses Mode::mode internally. Other parts of Mode,
  // such as Mode::origin, may have arbitrary values, and should be ignored.
  const chrome::search::Mode& mode() const { return mode_; }
  int height() const { return height_; }
  InstantSizeUnits height_units() const { return height_units_; }

  void SetOverlayState(const chrome::search::Mode& mode,
                       int height,
                       InstantSizeUnits height_units);

  void SetOverlayContents(content::WebContents* overlay_contents);
  content::WebContents* GetOverlayContents() const;

  // Add and remove observers.
  void AddObserver(InstantOverlayModelObserver* observer);
  void RemoveObserver(InstantOverlayModelObserver* observer);

 private:
  chrome::search::Mode mode_;
  int height_;
  InstantSizeUnits height_units_;

  // Weak. Remembers the last set overlay contents to detect changes. Actual
  // overlay contents is fetched from the |controller_| as this may not always
  // reflect the actual overlay in effect.
  content::WebContents* overlay_contents_;

  // Weak. The controller currently holds some model state.
  // TODO(dhollowa): Remove this, transfer all state to InstantOverlayModel.
  InstantController* const controller_;

  // Observers.
  ObserverList<InstantOverlayModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(InstantOverlayModel);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_MODEL_H_

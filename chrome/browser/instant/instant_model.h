// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_INSTANT_INSTANT_MODEL_H_
#define CHROME_BROWSER_INSTANT_INSTANT_MODEL_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/common/instant_types.h"

class InstantController;
class InstantModelObserver;
class TabContents;

// Holds state that is important to any views concerned with visibility and
// layout of the Instant preview.
class InstantModel {
 public:
  enum DisplayState {
    // When the preview contents are out of date, the preview should not be
    // shown.
    NOT_READY,

    // When the preview contents is updated and ready for display its state
    // transitions to |QUERY_RESULTS|.
    QUERY_RESULTS,
  };

  explicit InstantModel(InstantController* controller);
  ~InstantModel();

  void SetDisplayState(DisplayState display,
                       int height, InstantSizeUnits height_units);
  DisplayState display_state() const { return display_state_; }
  bool is_ready() const { return display_state_ != NOT_READY; }
  int height() const { return height_; }
  InstantSizeUnits height_units() const { return height_units_; }

  void SetPreviewContents(TabContents* preview_contents);
  TabContents* GetPreviewContents() const;

  // Add, remove, check observers.
  void AddObserver(InstantModelObserver* observer) const;
  void RemoveObserver(InstantModelObserver* observer) const;
  bool HasObserver(InstantModelObserver* observer) const;

 private:
  // |QUERY_RESULTS| if the preview should be displayed. Guaranteed to be
  // |NOT_READY| if InstantController::IsOutOfDate() is true.
  DisplayState display_state_;
  int height_;
  InstantSizeUnits height_units_;

  // Weak.  Remembers the last set preview contents to detect changes.
  // Actual preview contents is fetched from the |controller_| as this
  // may not always reflect the actual preview in effect.
  TabContents* preview_contents_;

  // Weak.  The controller currently holds some model state.
  // TODO(dhollowa): Remove this, transfer all model state to InstantModel.
  InstantController* const controller_;

  // Observers.
  mutable ObserverList<InstantModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(InstantModel);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_MODEL_H_

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
  enum PreviewState {
    // Contents are stale.
    NOT_READY,

    // Waiting for suggestions in response to the first omnibox interaction.
    AWAITING_SUGGESTIONS,

    // Showing search results and suggestions.
    QUERY_RESULTS,

    // Showing custom NTP content.
    CUSTOM_NTP_CONTENT,
  };

  explicit InstantModel(InstantController* controller);
  ~InstantModel();

  void SetPreviewState(PreviewState preview_state,
                       int height,
                       InstantSizeUnits height_units);
  PreviewState preview_state() const { return preview_state_; }
  int height() const { return height_; }
  InstantSizeUnits height_units() const { return height_units_; }

  void SetPreviewContents(TabContents* preview_contents);
  TabContents* GetPreviewContents() const;

  // Add, remove, check observers.
  void AddObserver(InstantModelObserver* observer) const;
  void RemoveObserver(InstantModelObserver* observer) const;
  bool HasObserver(InstantModelObserver* observer) const;

 private:
  PreviewState preview_state_;
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

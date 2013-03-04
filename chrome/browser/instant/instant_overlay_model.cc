// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_overlay_model.h"

#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_overlay_model_observer.h"

InstantOverlayModel::InstantOverlayModel(InstantController* controller)
  : height_(0),
    height_units_(INSTANT_SIZE_PIXELS),
    overlay_contents_(NULL),
    controller_(controller) {
}

InstantOverlayModel::~InstantOverlayModel() {
}

void InstantOverlayModel::SetOverlayState(const chrome::search::Mode& mode,
                                          int height,
                                          InstantSizeUnits height_units) {
  if (mode_.mode == mode.mode && height_ == height &&
      height_units_ == height_units) {
    // Mode::mode hasn't changed, but perhaps bits that we ignore (such as
    // Mode::origin) have. Update |mode_| anyway, so it's consistent with the
    // argument (so mode() doesn't return something unexpected).
    mode_ = mode;
    return;
  }

  DVLOG(1) << "SetOverlayState: " << mode_.mode << " to " << mode.mode;
  mode_ = mode;
  height_ = height;
  height_units_ = height_units;

  FOR_EACH_OBSERVER(InstantOverlayModelObserver, observers_,
                    OverlayStateChanged(*this));
}

void InstantOverlayModel::SetOverlayContents(
    content::WebContents* overlay_contents) {
  if (overlay_contents_ == overlay_contents)
    return;

  overlay_contents_ = overlay_contents;

  FOR_EACH_OBSERVER(InstantOverlayModelObserver, observers_,
                    OverlayStateChanged(*this));
}

content::WebContents* InstantOverlayModel::GetOverlayContents() const {
  // |controller_| maybe NULL durning tests.
  if (controller_)
    return controller_->GetOverlayContents();
  return overlay_contents_;
}

void InstantOverlayModel::AddObserver(InstantOverlayModelObserver* observer) {
  observers_.AddObserver(observer);
}

void InstantOverlayModel::RemoveObserver(
    InstantOverlayModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

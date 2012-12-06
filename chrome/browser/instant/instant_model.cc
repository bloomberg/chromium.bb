// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_model.h"

#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_model_observer.h"

InstantModel::InstantModel(InstantController* controller)
  : height_(0),
    height_units_(INSTANT_SIZE_PIXELS),
    preview_contents_(NULL),
    controller_(controller) {
}

InstantModel::~InstantModel() {
}

void InstantModel::SetPreviewState(const chrome::search::Mode& mode,
                                   int height,
                                   InstantSizeUnits height_units) {
  if (mode_.mode == mode.mode && height_ == height &&
      height_units_ == height_units) {
    // Mode::mode hasn't changed, but perhaps bits that we ignore (such as
    // Mode::origin) have. Update |mode_| anyway, so it's consistent with the
    // argument (so InstantModel::mode() doesn't return something unexpected).
    mode_ = mode;
    return;
  }

  DVLOG(1) << "SetPreviewState: " << mode_.mode << " to " << mode.mode;
  mode_ = mode;
  height_ = height;
  height_units_ = height_units;

  FOR_EACH_OBSERVER(InstantModelObserver, observers_,
                    PreviewStateChanged(*this));
}

void InstantModel::SetPreviewContents(content::WebContents* preview_contents) {
  if (preview_contents_ == preview_contents)
    return;

  preview_contents_ = preview_contents;

  FOR_EACH_OBSERVER(InstantModelObserver, observers_,
                    PreviewStateChanged(*this));
}

content::WebContents* InstantModel::GetPreviewContents() const {
  // |controller_| maybe NULL durning tests.
  if (controller_)
    return controller_->GetPreviewContents();
  return preview_contents_;
}

void InstantModel::AddObserver(InstantModelObserver* observer) const {
  observers_.AddObserver(observer);
}

void InstantModel::RemoveObserver(InstantModelObserver* observer) const {
  observers_.RemoveObserver(observer);
}

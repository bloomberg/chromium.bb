// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_model.h"

#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_model_observer.h"

InstantModel::InstantModel(InstantController* controller)
  : display_state_(NOT_READY),
    height_(0),
    height_units_(INSTANT_SIZE_PIXELS),
    preview_contents_(NULL),
    controller_(controller) {
}

InstantModel::~InstantModel() {
}

void InstantModel::SetDisplayState(DisplayState display_state,
                                   int height,
                                   InstantSizeUnits height_units) {
  if (display_state_ == display_state &&
      height_ == height &&
      height_units_ == height_units)
    return;

  display_state_ = display_state;
  height_ = height;
  height_units_ = height_units;

  FOR_EACH_OBSERVER(InstantModelObserver, observers_,
                    DisplayStateChanged(*this));
}

void InstantModel::SetPreviewContents(TabContents* preview_contents) {
  if (preview_contents_ == preview_contents)
    return;

  preview_contents_ = preview_contents;

  FOR_EACH_OBSERVER(InstantModelObserver, observers_,
                    DisplayStateChanged(*this));
}

TabContents* InstantModel::GetPreviewContents() const {
  return controller_->GetPreviewContents();
}

void InstantModel::AddObserver(InstantModelObserver* observer) const {
  observers_.AddObserver(observer);
}

void InstantModel::RemoveObserver(InstantModelObserver* observer) const {
  observers_.RemoveObserver(observer);
}

bool InstantModel::HasObserver(InstantModelObserver* observer) const {
  return observers_.HasObserver(observer);
}

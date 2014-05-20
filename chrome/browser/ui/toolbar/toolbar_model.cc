// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model.h"

#include "chrome/browser/search/search.h"

ToolbarModel::ToolbarModel()
    : input_in_progress_(false),
      origin_chip_enabled_(true),
      url_replacement_enabled_(true) {
}

ToolbarModel::~ToolbarModel() {
}

bool ToolbarModel::WouldReplaceURL() const {
  return WouldOmitURLDueToOriginChip() ||
      WouldPerformSearchTermReplacement(false);
}

bool ToolbarModel::ShouldShowOriginChip() const {
  return chrome::ShouldDisplayOriginChip() && WouldOmitURLDueToOriginChip() &&
      origin_chip_enabled();
}

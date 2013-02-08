// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"

#include "base/string16.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

ZoomDecoration::ZoomDecoration(ToolbarModel* toolbar_model)
    : toolbar_model_(toolbar_model) {
  Update(NULL);
}

ZoomDecoration::~ZoomDecoration() {
}

void ZoomDecoration::Update(ZoomController* zoom_controller) {
  if (!zoom_controller || zoom_controller->IsAtDefaultZoom() ||
      toolbar_model_->GetInputInProgress()) {
    // TODO(dbeam): hide zoom bubble when it exists.
    SetVisible(false);
    return;
  }

  SetImage(OmniboxViewMac::ImageForResource(
      zoom_controller->GetResourceForZoomLevel()));

  string16 zoom_percent = base::IntToString16(zoom_controller->zoom_percent());
  NSString* zoom_string =
      l10n_util::GetNSStringFWithFixup(IDS_TOOLTIP_ZOOM, zoom_percent);
  tooltip_.reset([zoom_string retain]);

  SetVisible(true);
}

bool ZoomDecoration::AcceptsMousePress() {
  return true;
}

NSString* ZoomDecoration::GetToolTip() {
  return tooltip_.get();
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"

#include "base/string16.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

ZoomDecoration::ZoomDecoration(LocationBarViewMac* owner)
    : owner_(owner),
      bubble_(nil) {
}

ZoomDecoration::~ZoomDecoration() {
  [bubble_ closeWithoutAnimation];
}

void ZoomDecoration::Update(ZoomController* zoom_controller) {
  if (!ShouldShowDecoration()) {
    [bubble_ close];
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

  [bubble_ onZoomChanged];
}

void ZoomDecoration::ToggleBubble(BOOL auto_close) {
  if (bubble_) {
    [bubble_ close];
    return;
  }

  content::WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents)
    return;

  // Get the frame of the decoration.
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  AutocompleteTextFieldCell* cell = [field cell];
  const NSRect frame = [cell frameForDecoration:this
                                        inFrame:[field bounds]];

  // Find point for bubble's arrow in screen coordinates.
  NSPoint anchor = GetBubblePointInFrame(frame);
  anchor = [field convertPoint:anchor toView:nil];
  anchor = [[field window] convertBaseToScreen:anchor];

  if (!bubble_) {
    void(^observer)(ZoomBubbleController*) = ^(ZoomBubbleController*) {
        bubble_ = nil;
        // If the page is at default zoom then hiding the zoom decoration was
        // suppressed while the bubble was open. Now that the bubble is closed
        // the decoration can be hidden.
        if (IsAtDefaultZoom())
          SetVisible(false);
    };
    bubble_ =
        [[ZoomBubbleController alloc] initWithParentWindow:[field window]
                                             closeObserver:observer];
  }

  [bubble_ showForWebContents:web_contents
                   anchoredAt:anchor
                    autoClose:auto_close];
}

NSPoint ZoomDecoration::GetBubblePointInFrame(NSRect frame) {
  return NSMakePoint(NSMaxX(frame), NSMaxY(frame));
}

bool ZoomDecoration::IsAtDefaultZoom() const {
  content::WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents)
    return false;
  ZoomController* zoomController =
      ZoomController::FromWebContents(web_contents);
  return zoomController && zoomController->IsAtDefaultZoom();
}

bool ZoomDecoration::ShouldShowDecoration() const {
  if (owner_->toolbar_model()->GetInputInProgress())
    return false;
  if (bubble_)
    return true;
  return !IsAtDefaultZoom();
}

bool ZoomDecoration::AcceptsMousePress() {
  return true;
}

bool ZoomDecoration::OnMousePressed(NSRect frame) {
  ToggleBubble(NO);
  return true;
}

NSString* ZoomDecoration::GetToolTip() {
  return tooltip_.get();
}

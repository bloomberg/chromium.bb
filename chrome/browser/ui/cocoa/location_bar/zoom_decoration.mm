// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/zoom/zoom_controller.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/l10n/l10n_util_mac.h"

ZoomDecoration::ZoomDecoration(LocationBarViewMac* owner)
    : owner_(owner), bubble_(nullptr), vector_icon_(nullptr) {}

ZoomDecoration::~ZoomDecoration() {
  [bubble_ closeWithoutAnimation];
  bubble_.delegate = nil;
}

bool ZoomDecoration::UpdateIfNecessary(zoom::ZoomController* zoom_controller,
                                       bool default_zoom_changed,
                                       bool location_bar_is_dark) {
  if (!ShouldShowDecoration()) {
    if (!IsVisible() && !bubble_)
      return false;

    HideUI();
    return true;
  }

  BOOL old_visibility = IsVisible();
  SetVisible(ShouldShowDecoration() && !zoom_controller->IsAtDefaultZoom());

  base::string16 zoom_percent =
      base::FormatPercent(zoom_controller->GetZoomPercent());
  // There is no icon at the default zoom factor (100%), so don't display a
  // tooltip either.
  NSString* tooltip_string =
      zoom_controller->IsAtDefaultZoom()
          ? @""
          : l10n_util::GetNSStringF(IDS_TOOLTIP_ZOOM, zoom_percent);

  if ([tooltip_ isEqualToString:tooltip_string] && !default_zoom_changed &&
      old_visibility == IsVisible()) {
    return false;
  }

  UpdateUI(zoom_controller, tooltip_string, location_bar_is_dark);
  return true;
}

void ZoomDecoration::ShowBubble(BOOL auto_close) {
  if (bubble_) {
    bubble_.delegate = nil;
    [[bubble_.window parentWindow] removeChildWindow:bubble_.window];
    [bubble_.window orderOut:nil];
    [bubble_ closeWithoutAnimation];
  }

  content::WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents)
    return;

  // Get the frame of the decoration.
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  const NSRect frame =
      [[field cell] frameForDecoration:this inFrame:[field bounds]];

  // Find point for bubble's arrow in screen coordinates.
  NSPoint anchor = GetBubblePointInFrame(frame);
  anchor = [field convertPoint:anchor toView:nil];
  anchor = ui::ConvertPointFromWindowToScreen([field window], anchor);

  bubble_ = [[ZoomBubbleController alloc] initWithParentWindow:[field window]
                                                      delegate:this];
  [bubble_ showAnchoredAt:anchor autoClose:auto_close];
}

void ZoomDecoration::CloseBubble() {
  [bubble_ close];
}

void ZoomDecoration::HideUI() {
  [bubble_ close];
  SetVisible(false);
}

void ZoomDecoration::UpdateUI(zoom::ZoomController* zoom_controller,
                              NSString* tooltip_string,
                              bool location_bar_is_dark) {
  vector_icon_ = nullptr;
  zoom::ZoomController::RelativeZoom relative_zoom =
      zoom_controller->GetZoomRelativeToDefault();
  // There is no icon at the default zoom factor.
  if (relative_zoom == zoom::ZoomController::ZOOM_BELOW_DEFAULT_ZOOM) {
    vector_icon_ = &kZoomMinusIcon;
  } else if (relative_zoom == zoom::ZoomController::ZOOM_ABOVE_DEFAULT_ZOOM) {
    vector_icon_ = &kZoomPlusIcon;
  }

  SetImage(GetMaterialIcon(location_bar_is_dark));

  tooltip_.reset([tooltip_string retain]);

  [bubble_ onZoomChanged];
}

NSPoint ZoomDecoration::GetBubblePointInFrame(NSRect frame) {
  return NSMakePoint(cocoa_l10n_util::ShouldDoExperimentalRTLLayout()
                         ? NSMinX(frame)
                         : NSMaxX(frame),
                     NSMaxY(frame));
}

bool ZoomDecoration::IsAtDefaultZoom() const {
  content::WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents)
    return false;

  zoom::ZoomController* zoomController =
      zoom::ZoomController::FromWebContents(web_contents);
  return zoomController && zoomController->IsAtDefaultZoom();
}

bool ZoomDecoration::ShouldShowDecoration() const {
  return owner_->GetWebContents() != NULL &&
      !owner_->GetToolbarModel()->input_in_progress() &&
      (bubble_ || !IsAtDefaultZoom());
}

bool ZoomDecoration::AcceptsMousePress() {
  return true;
}

bool ZoomDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  if (bubble_)
    CloseBubble();
  else
    ShowBubble(YES);
  return true;
}

NSString* ZoomDecoration::GetToolTip() {
  return tooltip_.get();
}

content::WebContents* ZoomDecoration::GetWebContents() {
  return owner_->GetWebContents();
}

void ZoomDecoration::OnClose() {
  bubble_.delegate = nil;
  bubble_ = nil;

  // If the page is at default zoom then hiding the zoom decoration
  // was suppressed while the bubble was open. Now that the bubble is
  // closed the decoration can be hidden.
  if (IsAtDefaultZoom() && IsVisible()) {
    SetVisible(false);
    owner_->OnDecorationsChanged();
  }
}

const gfx::VectorIcon* ZoomDecoration::GetMaterialVectorIcon() const {
  return vector_icon_;
}

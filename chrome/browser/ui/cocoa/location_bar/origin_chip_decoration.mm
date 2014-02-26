// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/origin_chip_decoration.h"

#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/origin_chip_info.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

const CGFloat kInnerLeftPadding = 3;
const CGFloat kIconLabelPadding = 2;
const CGFloat kInnerRightPadding = 6;
const CGFloat kOuterRightPadding = 3;

// The target icon size - smaller icons will be centered, and larger icons will
// be scaled down.
const CGFloat kIconSize = 19;

const ui::NinePartImageIds kDefaultNormalImageIds =
    IMAGE_GRID(IDR_ORIGIN_CHIP_NORMAL);
const ui::NinePartImageIds kDefaultHoverImageIds =
    IMAGE_GRID(IDR_ORIGIN_CHIP_HOVER);
const ui::NinePartImageIds kDefaultPressedImageIds =
    IMAGE_GRID(IDR_ORIGIN_CHIP_PRESSED);

// TODO(macourteau): add these once the assets have landed.
// const ui::NinePartImageIds kEVNormalImageIds = {};
// const ui::NinePartImageIds kEVHoverImageIds = {};
// const ui::NinePartImageIds kEVPressedImageIds = {};
// const ui::NinePartImageIds kBrokenSSLNormalImageIds = {};
// const ui::NinePartImageIds kBrokenSSLHoverImageIds = {};
// const ui::NinePartImageIds kBrokenSSLPressedImageIds = {};
// const ui::NinePartImageIds kMalwareNormalImageIds = {};
// const ui::NinePartImageIds kMalwareHoverImageIds = {};
// const ui::NinePartImageIds kMalwarePressedImageIds = {};

}  // namespace

OriginChipDecoration::OriginChipDecoration(LocationBarViewMac* owner)
    : ButtonDecoration(kDefaultNormalImageIds, IDR_LOCATION_BAR_HTTP,
                       kDefaultHoverImageIds, IDR_LOCATION_BAR_HTTP,
                       kDefaultPressedImageIds, IDR_LOCATION_BAR_HTTP, 0),
      attributes_([[NSMutableDictionary alloc] init]),
      info_(this, owner->browser()->profile()),
      owner_(owner) {
  DCHECK(owner_);
  [attributes_ setObject:GetFont() forKey:NSFontAttributeName];

  // May not be set for unit tests.
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service.get() && sb_service->ui_manager())
    sb_service->ui_manager()->AddObserver(this);
}

OriginChipDecoration::~OriginChipDecoration() {
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service.get() && sb_service->ui_manager())
    sb_service->ui_manager()->RemoveObserver(this);
}

void OriginChipDecoration::Update() {
  SetVisible(ShouldShow());

  const content::WebContents* web_contents =
      owner_->browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents || !info_.Update(web_contents, owner_->GetToolbarModel()))
    return;

  // TODO(macourteau): switch the background images and label color based on
  // the |info_.security_level()| and |info_.url_malware()|.

  label_.reset([base::SysUTF16ToNSString(info_.label()) retain]);
  SetIcon(info_.icon());
}

CGFloat OriginChipDecoration::GetWidthForSpace(CGFloat width) {
  NSImage* icon = GetIconImage();
  if (!icon || [label_ length] == 0)
    return kOmittedWidth;

  const CGFloat label_width = [label_ sizeWithAttributes:attributes_].width;
  const CGFloat min_width = kInnerLeftPadding +
                            kIconSize +
                            kIconLabelPadding +
                            std::ceil(label_width) +
                            kInnerRightPadding +
                            kOuterRightPadding;

  return (width < min_width) ? kOmittedWidth : min_width;
}

void OriginChipDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  // The assets are aligned with the location bar assets, so check that we are
  // being asked to draw against the edge, then draw over the border.
  DCHECK(NSMinX(frame) == [control_view cr_lineWidth]);
  frame = NSMakeRect(0, NSMinY(frame), NSWidth(frame) - kOuterRightPadding,
                     NSHeight(frame));

  const ui::NinePartImageIds image_ids = GetBackgroundImageIds();

  NSImage* icon =
      extension_icon_.get() ? extension_icon_.get() : GetIconImage();
  if (!icon)
    return;

  ui::DrawNinePartImage(frame, image_ids, NSCompositeSourceOver, 1.0, YES);

  const CGFloat icon_height = std::min([icon size].height, kIconSize);
  const CGFloat icon_width = std::min([icon size].width, kIconSize);
  CGFloat icon_y_inset = (NSHeight(frame) - icon_height) / 2.0;
  CGFloat icon_x_leading_offset = (kIconSize - icon_width) / 2.0;
  CGFloat icon_x_trailing_offset = icon_x_leading_offset;

  // Don't allow half-pixel offsets if we are not on a HiDPI display.
  if ([control_view cr_lineWidth] == 1.0) {
    icon_y_inset = std::ceil(icon_y_inset);
    icon_x_leading_offset = std::ceil(icon_x_leading_offset);
    icon_x_trailing_offset = std::floor(icon_x_trailing_offset);
  }

  NSRect icon_rect = NSMakeRect(kInnerLeftPadding + icon_x_leading_offset,
                                icon_y_inset, icon_width, icon_height);
  [icon drawInRect:icon_rect
          fromRect:NSZeroRect
         operation:NSCompositeSourceOver
          fraction:1.0
    respectFlipped:YES
             hints:nil];

  NSRect label_rect =
      NSMakeRect(NSMaxX(icon_rect) + icon_x_trailing_offset + kIconLabelPadding,
                 0, [label_ sizeWithAttributes:attributes_].width,
                 frame.size.height);
  DrawLabel(label_, attributes_, label_rect);
}

NSString* OriginChipDecoration::GetToolTip() {
  return label_.get();
}

bool OriginChipDecoration::OnMousePressed(NSRect frame) {
  // TODO(macourteau): reveal the permissions bubble if the click was inside the
  // icon's bounds - otherwise, show the URL.
  UMA_HISTOGRAM_COUNTS("OriginChip.Pressed", 1);
  content::RecordAction(base::UserMetricsAction("OriginChipPress"));
  owner_->GetOmniboxView()->ShowURL();
  return true;
}

void OriginChipDecoration::OnExtensionIconImageChanged(
      extensions::IconImage* image) {
  if (image) {
    extension_icon_.reset(gfx::Image(image->image_skia()).CopyNSImage());
    [owner_->GetAutocompleteTextField() setNeedsDisplay:YES];
  } else {
    extension_icon_.reset();
  }
}

// Note: When OnSafeBrowsingHit would be called, OnSafeBrowsingMatch will
// have already been called.
void OriginChipDecoration::OnSafeBrowsingHit(
    const SafeBrowsingUIManager::UnsafeResource& resource) {}

void OriginChipDecoration::OnSafeBrowsingMatch(
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  Update();
}

bool OriginChipDecoration::ShouldShow() const {
  return chrome::ShouldDisplayOriginChip() ||
      (owner_->GetToolbarModel()->WouldOmitURLDueToOriginChip() &&
       owner_->GetToolbarModel()->origin_chip_enabled());
}

// TODO(macourteau): Implement eliding of the host.

// TODO(macourteau): Implement dragging support.

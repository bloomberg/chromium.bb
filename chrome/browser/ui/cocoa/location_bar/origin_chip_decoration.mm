// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/origin_chip_decoration.h"

#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/search/search.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_icon_image.h"
#include "grit/theme_resources.h"
#include "ui/gfx/image/image.h"

namespace {

const CGFloat kInnerLeftPadding = 5;
const CGFloat kIconLabelPadding = 2;
const CGFloat kInnerRightPadding = 6;
const CGFloat kOuterRightPadding = 3;

// The target icon size - smaller icons will be centered, and larger icons will
// be scaled down.
const CGFloat kIconSize = 19;

// The info-bubble point should look like it points to the bottom of the lock
// icon. Determined with Pixie.app.
const CGFloat kPageInfoBubblePointYOffset = 2.0;

const ui::NinePartImageIds kNormalImages[3] = {
  IMAGE_GRID(IDR_ORIGIN_CHIP_NORMAL),
  IMAGE_GRID(IDR_ORIGIN_CHIP_HOVER),
  IMAGE_GRID(IDR_ORIGIN_CHIP_PRESSED)
};

const ui::NinePartImageIds kMalwareImages[3] = {
  IMAGE_GRID(IDR_ORIGIN_CHIP_MALWARE_NORMAL),
  IMAGE_GRID(IDR_ORIGIN_CHIP_MALWARE_HOVER),
  IMAGE_GRID(IDR_ORIGIN_CHIP_MALWARE_PRESSED)
};

const ui::NinePartImageIds kBrokenSSLImages[3] = {
  IMAGE_GRID(IDR_ORIGIN_CHIP_BROKENSSL_NORMAL),
  IMAGE_GRID(IDR_ORIGIN_CHIP_BROKENSSL_HOVER),
  IMAGE_GRID(IDR_ORIGIN_CHIP_BROKENSSL_PRESSED)
};

const ui::NinePartImageIds kEVImages[3] = {
  IMAGE_GRID(IDR_ORIGIN_CHIP_EV_NORMAL),
  IMAGE_GRID(IDR_ORIGIN_CHIP_EV_HOVER),
  IMAGE_GRID(IDR_ORIGIN_CHIP_EV_PRESSED)
};

}  // namespace

OriginChipDecoration::OriginChipDecoration(
    LocationBarViewMac* owner,
    LocationIconDecoration* location_icon)
    : ButtonDecoration(kNormalImages[0], IDR_LOCATION_BAR_HTTP,
                       kNormalImages[1], IDR_LOCATION_BAR_HTTP,
                       kNormalImages[2], IDR_LOCATION_BAR_HTTP, 0),
      attributes_([[NSMutableDictionary alloc] init]),
      icon_rect_(NSZeroRect),
      info_(this, owner->browser()->profile()),
      location_icon_(location_icon),
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
  if (!owner_->GetToolbarModel()->ShouldShowOriginChip()) {
    SetVisible(false);
    return;
  } else {
    SetVisible(true);
  }

  const content::WebContents* web_contents =
      owner_->browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents || !info_.Update(web_contents, owner_->GetToolbarModel()))
    return;

  if (info_.is_url_malware()) {
    SetBackgroundImageIds(kMalwareImages[0],
                          kMalwareImages[1],
                          kMalwareImages[2]);
  } else if (info_.security_level() == ToolbarModel::SECURITY_ERROR) {
    SetBackgroundImageIds(kBrokenSSLImages[0],
                          kBrokenSSLImages[1],
                          kBrokenSSLImages[2]);
  } else if (info_.security_level() == ToolbarModel::EV_SECURE) {
    SetBackgroundImageIds(kEVImages[0],
                          kEVImages[1],
                          kEVImages[2]);
  } else {
    SetBackgroundImageIds(kNormalImages[0],
                          kNormalImages[1],
                          kNormalImages[2]);
  }

  NSColor* foreground_color =
      info_.is_url_malware() ? [NSColor whiteColor] : [NSColor blackColor];
  [attributes_ setObject:foreground_color
                  forKey:NSForegroundColorAttributeName];

  label_.reset([base::SysUTF16ToNSString(info_.label()) retain]);
  SetIcon(info_.icon());
}

bool OriginChipDecoration::PreventFocus(NSPoint location) const {
  return NSPointInRect(location, icon_rect_) ? true : false;
}

CGFloat OriginChipDecoration::GetWidthForSpace(CGFloat width) {
  if (!GetIconImage() || [label_ length] == 0)
    return kOmittedWidth;

  // Clip the chip if it can't fit, rather than hiding it (kOmittedWidth).
  return std::min(width, GetChipWidth() + kOuterRightPadding);
}

void OriginChipDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  // The assets are aligned with the location bar assets, so check that we are
  // being asked to draw against the edge, then draw over the border.
  DCHECK(NSMinX(frame) == [control_view cr_lineWidth]);
  frame = NSMakeRect(0, NSMinY(frame), GetChipWidth(), NSHeight(frame));

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

  icon_rect_ = NSMakeRect(kInnerLeftPadding + icon_x_leading_offset,
                          icon_y_inset, icon_width, icon_height);

  [icon drawInRect:icon_rect_
          fromRect:NSZeroRect
         operation:NSCompositeSourceOver
          fraction:1.0
    respectFlipped:YES
             hints:nil];

  NSRect label_rect = NSMakeRect(
      NSMaxX(icon_rect_) + icon_x_trailing_offset + kIconLabelPadding,
      0, [label_ sizeWithAttributes:attributes_].width, frame.size.height);
  DrawLabel(label_, attributes_, label_rect);
}

NSString* OriginChipDecoration::GetToolTip() {
  return base::SysUTF16ToNSString(info_.Tooltip());
}

bool OriginChipDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  // Reveal the permissions bubble if the click was inside the icon's bounds;
  // otherwise, show the URL.
  if (NSPointInRect(location, icon_rect_))
    return location_icon_->OnMousePressed(frame, location);

  UMA_HISTOGRAM_COUNTS("OriginChip.Pressed", 1);
  content::RecordAction(base::UserMetricsAction("OriginChipPress"));
  owner_->GetOmniboxView()->ShowURL();
  return true;
}

NSPoint OriginChipDecoration::GetBubblePointInFrame(NSRect frame) {
  return NSMakePoint(NSMidX(icon_rect_),
                     NSMaxY(icon_rect_) - kPageInfoBubblePointYOffset);
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

CGFloat OriginChipDecoration::GetChipWidth() const {
  return kInnerLeftPadding +
      kIconSize +
      kIconLabelPadding +
      std::ceil([label_ sizeWithAttributes:attributes_].width) +
      kInnerRightPadding;
}

// TODO(macourteau): Implement eliding of the host.

// TODO(macourteau): Implement dragging support.

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/security_state_bubble_decoration.h"

#include <cmath>

#import "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/text_elider.h"

// TODO(spqchan): Decorations that don't fit in the available space are
// omitted. See crbug.com/638427.

namespace {

// Padding between the label and icon/divider.
CGFloat kLabelPadding = 4.0;

// Inset for the background.
const CGFloat kBackgroundYInset = 4.0;

// The offset of the text's baseline on a retina screen.
const CGFloat kRetinaBaselineOffset = 0.5;

// The info-bubble point should look like it points to the bottom of the lock
// icon. Determined with Pixie.app.
const CGFloat kPageInfoBubblePointYOffset = 2.0;

// Minimum acceptable width for the ev bubble.
const CGFloat kMinElidedBubbleWidth = 150.0;

// Maximum amount of available space to make the bubble, subject to
// |kMinElidedBubbleWidth|.
const float kMaxBubbleFraction = 0.5;

// Duration of animation in ms.
const NSTimeInterval kInAnimationDuration = 330;
const NSTimeInterval kOutAnimationDuration = 250;

// Transformation values at the beginning of the animation.
const CGFloat kStartScale = 0.25;
const CGFloat kStartx_offset = 15.0;

}  // namespace

//////////////////////////////////////////////////////////////////
// SecurityStateBubbleDecoration, public:

SecurityStateBubbleDecoration::SecurityStateBubbleDecoration(
    LocationIconDecoration* location_icon,
    LocationBarViewMac* owner)
    : location_icon_(location_icon),
      label_color_(gfx::kGoogleGreen700),
      image_fade_(true),
      animation_(this),
      owner_(owner),
      disable_animations_during_testing_(false) {
  // On Retina the text label is 1px above the Omnibox textfield's text
  // baseline. If the Omnibox textfield also drew the label the baselines
  // would align.
  SetRetinaBaselineOffset(kRetinaBaselineOffset);

  base::scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  [style setLineBreakMode:NSLineBreakByClipping];
  if (cocoa_l10n_util::ShouldDoExperimentalRTLLayout())
    [style setAlignment:NSRightTextAlignment];
  [attributes_ setObject:style forKey:NSParagraphStyleAttributeName];
  animation_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
}

SecurityStateBubbleDecoration::~SecurityStateBubbleDecoration() {
  // Just in case the timer is still holding onto the animation object, force
  // cleanup so it can't get back to |this|.
}

void SecurityStateBubbleDecoration::SetFullLabel(NSString* label) {
  full_label_.reset([label copy]);
  SetLabel(full_label_);
}

void SecurityStateBubbleDecoration::SetLabelColor(SkColor color) {
  label_color_ = color;
}

void SecurityStateBubbleDecoration::AnimateIn(bool image_fade) {
  image_fade_ = image_fade;
  if (HasAnimatedIn())
    animation_.Reset();

  animation_.SetSlideDuration(kInAnimationDuration);
  animation_.Show();
}

void SecurityStateBubbleDecoration::AnimateOut() {
  if (!HasAnimatedIn())
    return;

  animation_.SetSlideDuration(kOutAnimationDuration);
  animation_.Hide();
}

void SecurityStateBubbleDecoration::ShowWithoutAnimation() {
  animation_.Reset(1.0);
}

bool SecurityStateBubbleDecoration::HasAnimatedIn() const {
  return animation_.IsShowing() && animation_.GetCurrentValue() == 1.0;
}

bool SecurityStateBubbleDecoration::HasAnimatedOut() const {
  return !animation_.IsShowing() && animation_.GetCurrentValue() == 0.0;
}

bool SecurityStateBubbleDecoration::AnimatingOut() const {
  return !animation_.IsShowing() && animation_.GetCurrentValue() != 0.0;
}

void SecurityStateBubbleDecoration::ResetAnimation() {
  animation_.Reset();
}

//////////////////////////////////////////////////////////////////
// SecurityStateBubbleDecoration::LocationBarDecoration:

CGFloat SecurityStateBubbleDecoration::GetWidthForSpace(CGFloat width) {
  CGFloat location_icon_width = location_icon_->GetWidthForSpace(width);
  CGFloat text_width = GetWidthForText(width) - location_icon_width;
  return (text_width * GetAnimationProgress()) + location_icon_width;
}

void SecurityStateBubbleDecoration::DrawInFrame(NSRect frame,
                                                NSView* control_view) {
  const NSRect decoration_frame = NSInsetRect(frame, 0.0, kBackgroundYInset);
  CGFloat text_left_offset = NSMinX(decoration_frame);
  CGFloat text_right_offset = NSMaxX(decoration_frame);
  const BOOL is_rtl = cocoa_l10n_util::ShouldDoExperimentalRTLLayout();
  if (image_) {
    // The image should fade in if we're animating in.
    CGFloat image_alpha =
        image_fade_ && animation_.IsShowing() ? GetAnimationProgress() : 1.0;

    NSRect image_rect = GetImageRectInFrame(decoration_frame);
    [image_ drawInRect:image_rect
              fromRect:NSZeroRect  // Entire image
             operation:NSCompositeSourceOver
              fraction:image_alpha
        respectFlipped:YES
                 hints:nil];
    if (is_rtl) {
      text_left_offset += DividerPadding();
      text_right_offset = NSMinX(image_rect);
    } else {
      text_right_offset -= DividerPadding();
      text_left_offset = NSMaxX(image_rect);
    }
  }

  // Set the text color and draw the text.
  if (label_) {
    bool in_dark_mode = [[control_view window] inIncognitoModeWithSystemTheme];
    NSColor* text_color =
        in_dark_mode ? skia::SkColorToSRGBNSColor(kMaterialDarkModeTextColor)
                     : GetBackgroundBorderColor();
    SetTextColor(text_color);

    // Transform the coordinate system to adjust the baseline on Retina.
    // This is the only way to get fractional adjustments.
    gfx::ScopedNSGraphicsContextSaveGState save_graphics_state;
    CGFloat line_width = [control_view cr_lineWidth];
    if (line_width < 1) {
      NSAffineTransform* transform = [NSAffineTransform transform];
      [transform translateXBy:0 yBy:kRetinaBaselineOffset];
      [transform concat];
    }

    base::scoped_nsobject<NSAttributedString> text([[NSAttributedString alloc]
        initWithString:label_
            attributes:attributes_]);

    // Calculate the text frame based on the text height and offsets.
    NSRect text_rect = frame;
    CGFloat textHeight = [text size].height;

    text_rect.origin.x = text_left_offset;
    text_rect.origin.y = std::round(NSMidY(text_rect) - textHeight / 2.0) - 1;
    text_rect.size.width = text_right_offset - text_left_offset;
    text_rect.size.height = textHeight;
    text_rect = NSInsetRect(text_rect, kLabelPadding, 0);

    NSAffineTransform* transform = [NSAffineTransform transform];
    CGFloat progress = GetAnimationProgress();

    // Apply transformations so that the text animation:
    // - Scales from 0.75 to 1.
    // - Translates the X position to its origin after it got scaled, and
    //   before moving in a position from from -15 to 0
    // - Translates the Y position so that the text is centered vertically.
    double scale = gfx::Tween::DoubleValueBetween(progress, kStartScale, 1.0);

    double x_origin_offset = NSMinX(text_rect) * (1 - scale);
    double y_origin_offset = NSMinY(text_rect) * (1 - scale);
    double start_x_offset = is_rtl ? -kStartx_offset : kStartx_offset;
    double x_offset =
        gfx::Tween::DoubleValueBetween(progress, start_x_offset, 0);
    double y_offset = NSHeight(text_rect) * (1 - scale) / 2.0;

    [transform translateXBy:x_offset + x_origin_offset
                        yBy:y_offset + y_origin_offset];
    [transform scaleBy:scale];
    [transform concat];

    // Draw the label.
    [text drawInRect:text_rect];

    // Draw the divider.
    if (state() == DecorationMouseState::NONE && !active()) {
      const CGFloat divider_x_position =
          is_rtl ? NSMinX(decoration_frame) + DividerPadding()
                 : NSMaxX(decoration_frame) - DividerPadding();
      NSBezierPath* line = [NSBezierPath bezierPath];
      [line setLineWidth:line_width];
      [line moveToPoint:NSMakePoint(divider_x_position,
                                    NSMinY(decoration_frame))];
      [line lineToPoint:NSMakePoint(divider_x_position,
                                    NSMaxY(decoration_frame))];

      NSColor* divider_color = GetDividerColor(in_dark_mode);
      CGFloat divider_alpha =
          [divider_color alphaComponent] * GetAnimationProgress();
      divider_color = [divider_color colorWithAlphaComponent:divider_alpha];
      [divider_color set];
      [line stroke];
    }
  }
}

// Pass mouse operations through to location icon.
bool SecurityStateBubbleDecoration::IsDraggable() {
  return location_icon_->IsDraggable();
}

NSPasteboard* SecurityStateBubbleDecoration::GetDragPasteboard() {
  return location_icon_->GetDragPasteboard();
}

NSImage* SecurityStateBubbleDecoration::GetDragImage() {
  return location_icon_->GetDragImage();
}

NSRect SecurityStateBubbleDecoration::GetDragImageFrame(NSRect frame) {
  return GetImageRectInFrame(frame);
}

bool SecurityStateBubbleDecoration::OnMousePressed(NSRect frame,
                                                   NSPoint location) {
  return location_icon_->OnMousePressed(frame, location);
}

bool SecurityStateBubbleDecoration::AcceptsMousePress() {
  return true;
}

NSPoint SecurityStateBubbleDecoration::GetBubblePointInFrame(NSRect frame) {
  NSRect image_rect = GetImageRectInFrame(frame);
  return NSMakePoint(NSMidX(image_rect),
                     NSMaxY(image_rect) - kPageInfoBubblePointYOffset);
}

NSString* SecurityStateBubbleDecoration::GetToolTip() {
  NSString* tooltip_icon_text =
      l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_LOCATION_ICON);
  if ([full_label_ length] == 0)
    return tooltip_icon_text;
  return [NSString
      stringWithFormat:@"%@. %@", full_label_.get(), tooltip_icon_text];
}

//////////////////////////////////////////////////////////////////
// SecurityStateBubbleDecoration::BubbleDecoration:

NSColor* SecurityStateBubbleDecoration::GetBackgroundBorderColor() {
  return skia::SkColorToSRGBNSColor(
      SkColorSetA(label_color_, 255.0 * GetAnimationProgress()));
}

ui::NinePartImageIds SecurityStateBubbleDecoration::GetBubbleImageIds() {
  return IMAGE_GRID(IDR_OMNIBOX_EV_BUBBLE);
}

NSColor* SecurityStateBubbleDecoration::GetDarkModeTextColor() {
  return [NSColor whiteColor];
}

//////////////////////////////////////////////////////////////////
// SecurityStateBubbleDecoration::AnimationDelegate:

void SecurityStateBubbleDecoration::AnimationProgressed(
    const gfx::Animation* animation) {
  owner_->Layout();
}

//////////////////////////////////////////////////////////////////
// SecurityStateBubbleDecoration, private:

CGFloat SecurityStateBubbleDecoration::GetAnimationProgress() const {
  if (disable_animations_during_testing_)
    return 1.0;

  return animation_.GetCurrentValue();
}

CGFloat SecurityStateBubbleDecoration::GetWidthForText(CGFloat width) {
  // Limit with to not take up too much of the available width, but
  // also don't let it shrink too much.
  width = std::max(width * kMaxBubbleFraction, kMinElidedBubbleWidth);

  // Use the full label if it fits.
  NSImage* image = GetImage();
  const CGFloat all_width = GetWidthForImageAndLabel(image, full_label_);
  if (all_width <= width) {
    SetLabel(full_label_);
    return all_width;
  }

  // Width left for laying out the label.
  const CGFloat width_left = width - GetWidthForImageAndLabel(image, @"");

  // Middle-elide the label to fit |width_left|.  This leaves the
  // prefix and the trailing country code in place.
  NSString* elided_label = base::SysUTF16ToNSString(gfx::ElideText(
      base::SysNSStringToUTF16(full_label_),
      gfx::FontList(gfx::Font(GetFont())), width_left, gfx::ELIDE_MIDDLE));

  // Use the elided label.
  SetLabel(elided_label);
  return GetWidthForImageAndLabel(image, elided_label);
}

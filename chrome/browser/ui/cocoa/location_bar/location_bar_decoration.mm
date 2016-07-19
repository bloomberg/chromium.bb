// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// Color values for the Material bubble decoration divider.
const CGFloat kMaterialDividerAlpha = 38.0;
const CGFloat kMaterialDividerGrayScale = 0.0;
const CGFloat kMaterialDividerIncognitoGrayScale = 1.0;

}  // namespace

// A DecorationAccessibilityView is a focusable, pressable button that is fully
// transparent and cannot be hit by mouse clicks. This is overlaid over a drawn
// decoration, to fake keyboard focus on the decoration and make it visible to
// VoiceOver.
@interface DecorationAccessibilityView : NSButton {
  LocationBarDecoration* owner_;  // weak
}

// NSView:
- (id)initWithOwner:(LocationBarDecoration*)owner;
- (BOOL)acceptsFirstResponder;
- (void)drawRect:(NSRect)dirtyRect;
- (NSView*)hitTest:(NSPoint)aPoint;

// This method is called when this DecorationAccessibilityView is activated.
- (void)actionDidHappen;
@end

@implementation DecorationAccessibilityView

- (id)initWithOwner:(LocationBarDecoration*)owner {
  if ((self = [super initWithFrame:NSZeroRect])) {
    self.bordered = NO;
    self.focusRingType = NSFocusRingTypeExterior;
    self->owner_ = owner;
    [self setAction:@selector(actionDidHappen)];
    [self setTarget:self];
  }
  return self;
}

- (BOOL)acceptsFirstResponder {
  // This NSView is only focusable if the owning LocationBarDecoration can
  // accept mouse presses.
  // TODO(ellyjones): Once the key view loop order in ToolbarController is fixed
  // up properly (which will require some redesign of
  // LocationBarViewMac::GetDecorationAccessibilityViews()), this method should
  // honor |owner_->AcceptsMousePress()|. See https://crbug.com/623883.
  return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
  // Draw nothing. This NSView is fully transparent.
}

- (NSView*)hitTest:(NSPoint)aPoint {
  // Mouse clicks cannot hit this NSView.
  return nil;
}

- (void)actionDidHappen {
  owner_->OnAccessibilityViewAction();
}

@end

const CGFloat LocationBarDecoration::kOmittedWidth = 0.0;
const SkColor LocationBarDecoration::kMaterialDarkModeTextColor = 0xCCFFFFFF;

LocationBarDecoration::LocationBarDecoration() {
  accessibility_view_.reset(
      [[DecorationAccessibilityView alloc] initWithOwner:this]);
  [accessibility_view_.get() setHidden:YES];
}

void LocationBarDecoration::OnAccessibilityViewAction() {
  // Turn the action into a synthesized mouse click at the center of |this|.
  NSRect frame = [accessibility_view_.get() frame];
  NSPoint mousePoint = NSMakePoint(NSMidX(frame), NSMidY(frame));
  OnMousePressed(frame, mousePoint);
}

LocationBarDecoration::~LocationBarDecoration() {
  [accessibility_view_.get() removeFromSuperview];
}

bool LocationBarDecoration::IsVisible() const {
  return visible_;
}

void LocationBarDecoration::SetVisible(bool visible) {
  visible_ = visible;
  [accessibility_view_.get() setHidden:visible ? NO : YES];
}


CGFloat LocationBarDecoration::GetWidthForSpace(CGFloat width) {
  NOTREACHED();
  return kOmittedWidth;
}

void LocationBarDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  NOTREACHED();
}

void LocationBarDecoration::DrawWithBackgroundInFrame(NSRect background_frame,
                                                      NSRect frame,
                                                      NSView* control_view) {
  // Default to no background.
  DrawInFrame(frame, control_view);
}

NSString* LocationBarDecoration::GetToolTip() {
  return nil;
}

bool LocationBarDecoration::AcceptsMousePress() {
  return false;
}

bool LocationBarDecoration::IsDraggable() {
  return false;
}

NSImage* LocationBarDecoration::GetDragImage() {
  return nil;
}

NSRect LocationBarDecoration::GetDragImageFrame(NSRect frame) {
  return NSZeroRect;
}

NSPasteboard* LocationBarDecoration::GetDragPasteboard() {
  return nil;
}

bool LocationBarDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  return false;
}

NSMenu* LocationBarDecoration::GetMenu() {
  return nil;
}

NSFont* LocationBarDecoration::GetFont() const {
  return OmniboxViewMac::GetNormalFieldFont();
}

NSView* LocationBarDecoration::GetAccessibilityView() {
  return accessibility_view_.get();
}

NSPoint LocationBarDecoration::GetBubblePointInFrame(NSRect frame) {
  // Clients that use a bubble should implement this. Can't be abstract
  // because too many LocationBarDecoration subclasses don't use a bubble.
  // Can't live on subclasses only because it needs to be on a shared API.
  NOTREACHED();
  return frame.origin;
}

NSImage* LocationBarDecoration::GetMaterialIcon(
    bool location_bar_is_dark) const {
  const int kIconSize = 16;
  gfx::VectorIconId vector_icon_id = GetMaterialVectorIconId();
  if (vector_icon_id == gfx::VectorIconId::VECTOR_ICON_NONE) {
    // Return an empty image when the decoration specifies no vector icon, so
    // that its bubble is positioned correctly (the position is based on the
    // width of the image; returning nil will mess up the positioning).
    NSSize icon_size = NSMakeSize(kIconSize, kIconSize);
    return [[[NSImage alloc] initWithSize:icon_size] autorelease];
  }

  SkColor vector_icon_color = GetMaterialIconColor(location_bar_is_dark);
  return NSImageFromImageSkia(
      gfx::CreateVectorIcon(vector_icon_id, kIconSize, vector_icon_color));
}

// static
void LocationBarDecoration::DrawLabel(NSString* label,
                                      NSDictionary* attributes,
                                      const NSRect& frame) {
  base::scoped_nsobject<NSAttributedString> str(
      [[NSAttributedString alloc] initWithString:label attributes:attributes]);
  DrawAttributedString(str, frame);
}

// static
void LocationBarDecoration::DrawAttributedString(NSAttributedString* str,
                                                 const NSRect& frame) {
  NSRect text_rect = frame;
  text_rect.size.height = [str size].height;
  text_rect.origin.y = roundf(NSMidY(frame) - NSHeight(text_rect) / 2.0) - 1;
  [str drawInRect:text_rect];
}

// static
NSSize LocationBarDecoration::GetLabelSize(NSString* label,
                                           NSDictionary* attributes) {
  return [label sizeWithAttributes:attributes];
}

SkColor LocationBarDecoration::GetMaterialIconColor(
    bool location_bar_is_dark) const {
  return location_bar_is_dark ? SK_ColorWHITE : gfx::kChromeIconGrey;
}

NSColor* LocationBarDecoration::GetDividerColor(
    bool location_bar_is_dark) const {
  CGFloat gray_scale = location_bar_is_dark ? kMaterialDividerIncognitoGrayScale
                                            : kMaterialDividerGrayScale;
  return [NSColor colorWithCalibratedWhite:gray_scale
                                     alpha:kMaterialDividerAlpha / 255.0];
}

gfx::VectorIconId LocationBarDecoration::GetMaterialVectorIconId() const {
  NOTREACHED();
  return gfx::VectorIconId::VECTOR_ICON_NONE;
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/tracking_area.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// Color values for the bubble decoration divider.
const CGFloat kDividerAlpha = 38.0;
const CGFloat kDividerGrayScale = 0.0;
const CGFloat kDividerIncognitoGrayScale = 1.0;

// Color values for the hover and pressed background.
const SkColor kHoverBackgroundColor = 0x14000000;
const SkColor kHoverDarkBackgroundColor = 0x1EFFFFFF;
const SkColor kPressedBackgroundColor = 0x1E000000;
const SkColor kPressedDarkBackgroundColor = 0x3DFFFFFF;

// Amount of inset for the background frame.
const CGFloat kBackgroundFrameYInset = 2.0;

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
- (NSString*)accessibilityLabel;

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

- (NSString*)accessibilityLabel {
  return owner_->GetToolTip();
}

@end

@interface DecorationMouseTrackingDelegate : NSObject {
  LocationBarDecoration* owner_;  // weak
}

- (id)initWithOwner:(LocationBarDecoration*)owner;
- (void)mouseEntered:(NSEvent*)event;
- (void)mouseExited:(NSEvent*)event;

@end

@implementation DecorationMouseTrackingDelegate

- (id)initWithOwner:(LocationBarDecoration*)owner {
  if ((self = [super init])) {
    owner_ = owner;
  }

  return self;
}

- (void)mouseEntered:(NSEvent*)event {
  owner_->OnMouseEntered();
}

- (void)mouseExited:(NSEvent*)event {
  owner_->OnMouseExited();
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

NSRect LocationBarDecoration::GetBackgroundFrame(NSRect frame) {
  return NSInsetRect(frame, 0.0, kBackgroundFrameYInset);
}

void LocationBarDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  NOTREACHED();
}

void LocationBarDecoration::DrawWithBackgroundInFrame(NSRect frame,
                                                      NSView* control_view) {
  // Draw the background if available.
  if ((active_ || state_ != DecorationMouseState::NONE) &&
      HasHoverAndPressEffect()) {
    bool in_dark_mode = [[control_view window] inIncognitoModeWithSystemTheme];

    SkColor background_color;
    if (active_ || state_ == DecorationMouseState::PRESSED) {
      background_color =
          in_dark_mode ? kPressedDarkBackgroundColor : kPressedBackgroundColor;
    } else {
      background_color =
          in_dark_mode ? kHoverDarkBackgroundColor : kHoverBackgroundColor;
    }

    [skia::SkColorToSRGBNSColor(background_color) setFill];

    NSBezierPath* path = [NSBezierPath bezierPath];
    [path appendBezierPathWithRoundedRect:GetBackgroundFrame(frame)
                                  xRadius:2
                                  yRadius:2];
    [path fill];
  }
  DrawInFrame(frame, control_view);
}

NSString* LocationBarDecoration::GetToolTip() {
  return nil;
}

CrTrackingArea* LocationBarDecoration::SetupTrackingArea(NSRect frame,
                                                         NSView* control_view) {
  if (!AcceptsMousePress() || !control_view)
    return nil;

  if (control_view == tracking_area_owner_ && tracking_area_.get() &&
      NSEqualRects([tracking_area_ rect], frame)) {
    return tracking_area_.get();
  }

  if (tracking_area_owner_)
    [tracking_area_owner_ removeTrackingArea:tracking_area_.get()];

  if (!tracking_delegate_) {
    tracking_delegate_.reset(
        [[DecorationMouseTrackingDelegate alloc] initWithOwner:this]);
  }

  tracking_area_.reset([[CrTrackingArea alloc]
      initWithRect:frame
           options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow
             owner:tracking_delegate_.get()
          userInfo:nil]);

  [control_view addTrackingArea:tracking_area_.get()];
  tracking_area_owner_ = control_view;

  state_ = [tracking_area_ mouseInsideTrackingAreaForView:control_view]
               ? DecorationMouseState::HOVER
               : DecorationMouseState::NONE;

  return tracking_area_.get();
}

void LocationBarDecoration::RemoveTrackingArea() {
  DCHECK(tracking_area_owner_);
  DCHECK(tracking_area_);
  [tracking_area_owner_ removeTrackingArea:tracking_area_.get()];
  tracking_area_owner_ = nullptr;
  tracking_area_.reset();
}

bool LocationBarDecoration::AcceptsMousePress() {
  return false;
}

bool LocationBarDecoration::HasHoverAndPressEffect() {
  return AcceptsMousePress();
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

void LocationBarDecoration::OnMouseDown() {
  state_ = DecorationMouseState::PRESSED;
  UpdateDecorationState();
}

void LocationBarDecoration::OnMouseUp() {
  DCHECK(tracking_area_owner_);
  state_ = [tracking_area_ mouseInsideTrackingAreaForView:tracking_area_owner_]
               ? DecorationMouseState::HOVER
               : DecorationMouseState::NONE;
  UpdateDecorationState();
}

void LocationBarDecoration::OnMouseEntered() {
  state_ = DecorationMouseState::HOVER;
  UpdateDecorationState();
}

void LocationBarDecoration::OnMouseExited() {
  state_ = DecorationMouseState::NONE;
  UpdateDecorationState();
}

void LocationBarDecoration::SetActive(bool active) {
  if (active_ == active)
    return;

  active_ = active;
  state_ = [tracking_area_ mouseInsideTrackingAreaForView:tracking_area_owner_]
               ? DecorationMouseState::HOVER
               : DecorationMouseState::NONE;
  UpdateDecorationState();
}

void LocationBarDecoration::UpdateDecorationState() {
  if (tracking_area_owner_)
    [tracking_area_owner_ setNeedsDisplay:YES];
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
  CGFloat gray_scale =
      location_bar_is_dark ? kDividerIncognitoGrayScale : kDividerGrayScale;
  return
      [NSColor colorWithCalibratedWhite:gray_scale alpha:kDividerAlpha / 255.0];
}

gfx::VectorIconId LocationBarDecoration::GetMaterialVectorIconId() const {
  NOTREACHED();
  return gfx::VectorIconId::VECTOR_ICON_NONE;
}

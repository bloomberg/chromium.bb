// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_item_cell.h"

#include "app/gfx/text_elider.h"
#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/download_item_cell.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

namespace {

// Distance from top border to icon
const CGFloat kImagePaddingTop = 1;

// Distance from left border to icon
const CGFloat kImagePaddingLeft = 1;

// Width of icon
const CGFloat kImageWidth = 32;

// Height of icon
const CGFloat kImageHeight = 32;

// x coordinate of download name string, in view coords
const CGFloat kTextPosLeft = kImagePaddingLeft + kImageWidth + 1;

// Distance from end of download name string to dropdown area
const CGFloat kTextPaddingRight = 3;

// y coordinate of download name string, in view coords, when status message
// is visible
const CGFloat kPrimaryTextPosTop = 5;

// y coordinate of download name string, in view coords, when status message
// is not visible
const CGFloat kPrimaryTextOnlyPosTop = 10;

// y coordinate of status message, in view coords
const CGFloat kSecondaryTextPosTop = 17;

// Width of dropdown area on the right
const CGFloat kDropdownAreaWidth = 18;

// Width of dropdown arrow
const CGFloat kDropdownArrowWidth = 5;

// Height of dropdown arrow
const CGFloat kDropdownArrowHeight = 3;

// Duration of the two-lines-to-one-line animation, in seconds
NSTimeInterval kHideStatusDuration = 0.3;

}

// This is a helper class to animate the fading out of the status text.
@interface HideSecondaryTitleAnimation : NSAnimation {
  DownloadItemCell* cell_;
}
- (id)initWithDownloadItemCell:(DownloadItemCell*)cell;
@end

@interface DownloadItemCell(Private)
- (void)updateTrackingAreas:(id)sender;
- (void)hideSecondaryTitle;
- (void)animationProgressed:(NSAnimationProgress)progress;
@end

@implementation DownloadItemCell

@synthesize secondaryTitle = secondaryTitle_;
@synthesize secondaryFont = secondaryFont_;

- (void)setInitialState {
  isStatusTextVisible_ = NO;
  titleY_ = kPrimaryTextPosTop;
  statusAlpha_ = 1.0;

  [self setFont:[NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSSmallControlSize]]];
  [self setSecondaryFont:[NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSMiniControlSize]]];

  [self updateTrackingAreas:self];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateTrackingAreas:)
             name:NSViewFrameDidChangeNotification
           object:[self controlView]];
}

// For nib instantiations
- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    [self setInitialState];
  }
  return self;
}

// For programmatic instantiations
- (id)initTextCell:(NSString *)string {
  if ((self = [super initTextCell:string])) {
    [self setInitialState];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [secondaryTitle_ release];
  [secondaryFont_ release];
  [super dealloc];
}

- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel {
  // Set name and icon of download.
  downloadPath_ = downloadModel->download()->GetFileName();

  // TODO(paulg): Use IconManager for loading icons on the file thread
  // (crbug.com/16226).
  NSString* extension = base::SysUTF8ToNSString(downloadPath_.Extension());
  NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFileType:extension];
  [self setImage:icon];

  std::wstring statusText = downloadModel->GetStatusText();
  if (statusText.empty()) {
    // Remove the status text label.
    [self hideSecondaryTitle];
    isStatusTextVisible_ = NO;
  } else {
    // Set status text.
    NSString* statusString = base::SysWideToNSString(statusText);
    [self setSecondaryTitle:statusString];
    isStatusTextVisible_ = YES;
  }
}

- (void)updateTrackingAreas:(id)sender {
  if (trackingAreaButton_) {
    [[self controlView] removeTrackingArea:trackingAreaButton_.get()];
      trackingAreaButton_.reset(nil);
  }
  if (trackingAreaDropdown_) {
    [[self controlView] removeTrackingArea:trackingAreaDropdown_.get()];
      trackingAreaDropdown_.reset(nil);
  }

  // Use two distinct tracking rects for left and right parts.
  NSRect bounds = [[self controlView] bounds];
  NSRect buttonRect, dropdownRect;
  NSDivideRect(bounds, &dropdownRect, &buttonRect,
      kDropdownAreaWidth, NSMaxXEdge);

    trackingAreaButton_.reset([[NSTrackingArea alloc]
                    initWithRect:buttonRect
                         options:(NSTrackingMouseEnteredAndExited |
                                  NSTrackingActiveInActiveApp)
                           owner:self
                      userInfo:nil]);
  [[self controlView] addTrackingArea:trackingAreaButton_.get()];

    trackingAreaDropdown_.reset([[NSTrackingArea alloc]
                    initWithRect:dropdownRect
                         options:(NSTrackingMouseEnteredAndExited |
                                  NSTrackingActiveInActiveApp)
                           owner:self
                      userInfo:nil]);
  [[self controlView] addTrackingArea:trackingAreaDropdown_.get()];
}

- (void)setShowsBorderOnlyWhileMouseInside:(BOOL)showOnly {
  // Override to make sure it doesn't do anything if it's called accidentally.
}

- (void)mouseEntered:(NSEvent*)theEvent {
  mouseInsideCount_++;
  if ([theEvent trackingArea] == trackingAreaButton_.get())
    mousePosition_ = kDownloadItemMouseOverButtonPart;
  else if ([theEvent trackingArea] == trackingAreaDropdown_.get())
    mousePosition_ = kDownloadItemMouseOverDropdownPart;
  [[self controlView] setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent *)theEvent {
  mouseInsideCount_--;
  if (mouseInsideCount_ == 0)
    mousePosition_ = kDownloadItemMouseOutside;
  [[self controlView] setNeedsDisplay:YES];
}

- (BOOL)isMouseInside {
  return mousePosition_ != kDownloadItemMouseOutside;
}

- (BOOL)isMouseOverButtonPart {
  return mousePosition_ == kDownloadItemMouseOverButtonPart;
}

- (BOOL)isButtonPartPressed {
  return [self isHighlighted]
      && mousePosition_ == kDownloadItemMouseOverButtonPart;
}

- (BOOL)isMouseOverDropdownPart {
  return mousePosition_ == kDownloadItemMouseOverDropdownPart;
}

- (BOOL)isDropdownPartPressed {
  return [self isHighlighted]
      && mousePosition_ == kDownloadItemMouseOverDropdownPart;
}

- (NSBezierPath*)leftRoundedPath:(CGFloat)radius inRect:(NSRect)rect {

  NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
  NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
  NSPoint bottomRight = NSMakePoint(NSMaxX(rect) , NSMinY(rect));

  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:topRight];
  [path appendBezierPathWithArcFromPoint:topLeft
                                 toPoint:rect.origin
                                  radius:radius];
  [path appendBezierPathWithArcFromPoint:rect.origin
                                 toPoint:bottomRight
                                 radius:radius];
  [path lineToPoint:bottomRight];
  return path;
}

- (NSBezierPath*)rightRoundedPath:(CGFloat)radius inRect:(NSRect)rect {

  NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
  NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
  NSPoint bottomRight = NSMakePoint(NSMaxX(rect), NSMinY(rect));

  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:rect.origin];
  [path appendBezierPathWithArcFromPoint:bottomRight
                                toPoint:topRight
                                  radius:radius];
  [path appendBezierPathWithArcFromPoint:topRight
                                toPoint:topLeft
                                 radius:radius];
  [path lineToPoint:topLeft];
  [path closePath];  // Right path is closed
  return path;
}

- (void)elideTitle:(int)availableWidth {
  NSFont* font = [self font];
  gfx::Font font_chr =
      gfx::Font::CreateFont(base::SysNSStringToWide([font fontName]),
                            [font pointSize]);

  NSString* titleString = base::SysWideToNSString(
      ElideFilename(downloadPath_, font_chr, availableWidth));
  [self setTitle:titleString];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {

  // Constants from Cole.  Will kConstan them once the feedback loop
  // is complete.
  NSRect drawFrame = NSInsetRect(cellFrame, 0.5, 0.5);
  NSRect innerFrame = NSInsetRect(cellFrame, 1, 1);

  const float radius = 3.5;
  NSWindow* window = [controlView window];
  BOOL active = [window isKeyWindow] || [window isMainWindow];

  GTMTheme* theme = [controlView gtm_theme];

  NSRect buttonDrawRect, dropdownDrawRect;
  NSDivideRect(drawFrame, &dropdownDrawRect, &buttonDrawRect,
      kDropdownAreaWidth, NSMaxXEdge);

  NSRect buttonInnerRect, dropdownInnerRect;
  NSDivideRect(innerFrame, &dropdownInnerRect, &buttonInnerRect,
      kDropdownAreaWidth, NSMaxXEdge);

  NSBezierPath* buttonInnerPath = [self
      leftRoundedPath:radius inRect:buttonDrawRect];
  NSBezierPath* buttonOuterPath = [self
      leftRoundedPath:(radius + 1)
               inRect:NSInsetRect(buttonDrawRect, -1, -1)];

  NSBezierPath* dropdownInnerPath = [self
      rightRoundedPath:radius inRect:dropdownDrawRect];
  NSBezierPath* dropdownOuterPath = [self
      rightRoundedPath:(radius + 1)
                inRect:NSInsetRect(dropdownDrawRect, -1, -1)];

  // Stroke the borders and appropriate fill gradient. If we're borderless,
  // the only time we want to draw the inner gradient is if we're highlighted.
  if ([self isHighlighted] || [self isMouseInside]) {
    [self drawBorderAndFillForTheme:theme
                        controlView:controlView
                          outerPath:buttonOuterPath
                          innerPath:buttonInnerPath
              showHighlightGradient:[self isMouseOverButtonPart]
                showClickedGradient:[self isButtonPartPressed]
                             active:active
                          cellFrame:cellFrame];

    [self drawBorderAndFillForTheme: theme
                        controlView:controlView
                          outerPath:dropdownOuterPath
                          innerPath:dropdownInnerPath
              showHighlightGradient:[self isMouseOverDropdownPart]
                showClickedGradient:[self isDropdownPartPressed]
                             active:active
                          cellFrame:cellFrame];
  }

  [self drawInteriorWithFrame:innerFrame inView:controlView];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // Draw title
  [self elideTitle:cellFrame.size.width -
      (kTextPosLeft + kTextPaddingRight + kDropdownAreaWidth)];

  NSColor* color = [self isButtonPartPressed]
      ? [NSColor alternateSelectedControlTextColor] : [NSColor textColor];
  NSString* primaryText = [self title];

  NSDictionary* primaryTextAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:
      color, NSForegroundColorAttributeName,
      [self font], NSFontAttributeName,
      nil];
  NSPoint primaryPos = NSMakePoint(
      cellFrame.origin.x + kTextPosLeft,
      titleY_);

  [primaryText drawAtPoint:primaryPos withAttributes:primaryTextAttributes];

  // Draw secondary title, if any
  if ([self secondaryTitle] != nil && statusAlpha_ > 0) {
    NSString* secondaryText = [self secondaryTitle];
    NSColor* secondaryColor = [color colorWithAlphaComponent:statusAlpha_];
    NSDictionary* secondaryTextAttributes = [NSDictionary
        dictionaryWithObjectsAndKeys:
        secondaryColor, NSForegroundColorAttributeName,
        [self secondaryFont], NSFontAttributeName,
        nil];
    NSPoint secondaryPos = NSMakePoint(
        cellFrame.origin.x + kTextPosLeft,
        kSecondaryTextPosTop);
    [secondaryText drawAtPoint:secondaryPos
        withAttributes:secondaryTextAttributes];
  }

  // Draw icon
  NSRect imageRect = NSZeroRect;
  imageRect.size = [[self image] size];
  [[self image] setFlipped:[controlView isFlipped]];
  [[self image] drawInRect:[self imageRectForBounds:cellFrame]
                  fromRect:imageRect
                 operation:NSCompositeSourceOver
                  fraction:[self isEnabled] ? 1.0 : 0.5];

  // Popup arrow. Put center of mass of the arrow in the center of the
  // dropdown area.
  CGFloat cx = NSMaxX(cellFrame) - kDropdownAreaWidth/2;
  CGFloat cy = NSMidY(cellFrame);
  NSPoint p1 = NSMakePoint(cx - kDropdownArrowWidth/2,
                           cy - kDropdownArrowHeight/3);
  NSPoint p2 = NSMakePoint(cx + kDropdownArrowWidth/2,
                           cy - kDropdownArrowHeight/3);
  NSPoint p3 = NSMakePoint(cx, cy + kDropdownArrowHeight*2/3);
  NSBezierPath *triangle = [NSBezierPath bezierPath];
  [triangle moveToPoint:p1];
  [triangle lineToPoint:p2];
  [triangle lineToPoint:p3];
  [triangle closePath];

  NSColor* fill = [self isDropdownPartPressed]
      ? [NSColor alternateSelectedControlTextColor] : [NSColor textColor];
  [fill setFill];
  [triangle fill];
}

- (NSRect)imageRectForBounds:(NSRect)cellFrame {
  return NSMakeRect(
      kImagePaddingLeft, kImagePaddingTop, kImageWidth, kImageHeight);
}

- (void)hideSecondaryTitle {
  if (isStatusTextVisible_) {
    // No core animation -- text in CA layers is not subpixel antialiased :-/
    hideStatusAnimation_.reset([[HideSecondaryTitleAnimation alloc]
        initWithDownloadItemCell:self]);
    [hideStatusAnimation_.get() setDelegate:self];
    [hideStatusAnimation_.get() startAnimation];
  } else {
    // If the download is done so quickly that the status line is never visible,
    // don't show an animation
    [self animationProgressed:1.0];
  }
}

- (void)animationProgressed:(NSAnimationProgress)progress {
  titleY_ = progress*kPrimaryTextOnlyPosTop + (1 - progress)*kPrimaryTextPosTop;
  statusAlpha_ = 1 - progress;
  [[self controlView] setNeedsDisplay:YES];
}

- (void)animationDidEnd:(NSAnimation *)animation {
  hideStatusAnimation_.reset();
}

@end

@implementation HideSecondaryTitleAnimation

- (id)initWithDownloadItemCell:(DownloadItemCell*)cell {
  if ((self = [super initWithDuration:kHideStatusDuration
                       animationCurve:NSAnimationEaseIn])) {
    cell_ = cell;
    [self setAnimationBlockingMode:NSAnimationNonblocking];
  }
  return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  [super setCurrentProgress:progress];
  [cell_ animationProgressed:progress];
}

@end

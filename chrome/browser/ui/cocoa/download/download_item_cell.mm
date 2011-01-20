// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_item_cell.h"

#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#import "chrome/browser/themes/browser_theme_provider.h"
#import "chrome/browser/ui/cocoa/download/download_item_cell.h"
#import "chrome/browser/ui/cocoa/image_utils.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "gfx/canvas_skia_paint.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"
#include "ui/base/text/text_elider.h"

namespace {

// Distance from top border to icon
const CGFloat kImagePaddingTop = 7;

// Distance from left border to icon
const CGFloat kImagePaddingLeft = 9;

// Width of icon
const CGFloat kImageWidth = 16;

// Height of icon
const CGFloat kImageHeight = 16;

// x coordinate of download name string, in view coords
const CGFloat kTextPosLeft = kImagePaddingLeft +
    kImageWidth + download_util::kSmallProgressIconOffset;

// Distance from end of download name string to dropdown area
const CGFloat kTextPaddingRight = 3;

// y coordinate of download name string, in view coords, when status message
// is visible
const CGFloat kPrimaryTextPosTop = 3;

// y coordinate of download name string, in view coords, when status message
// is not visible
const CGFloat kPrimaryTextOnlyPosTop = 10;

// y coordinate of status message, in view coords
const CGFloat kSecondaryTextPosTop = 18;

// Grey value of status text
const CGFloat kSecondaryTextColor = 0.5;

// Width of dropdown area on the right (includes 1px for the border on each
// side).
const CGFloat kDropdownAreaWidth = 14;

// Width of dropdown arrow
const CGFloat kDropdownArrowWidth = 5;

// Height of dropdown arrow
const CGFloat kDropdownArrowHeight = 3;

// Vertical displacement of dropdown area, relative to the "centered" position.
const CGFloat kDropdownAreaY = -2;

// Duration of the two-lines-to-one-line animation, in seconds
NSTimeInterval kHideStatusDuration = 0.3;

// Duration of the 'download complete' animation, in seconds
const int kCompleteAnimationDuration = 2.5;

}

// This is a helper class to animate the fading out of the status text.
@interface DownloadItemCellAnimation : NSAnimation {
  DownloadItemCell* cell_;
}
- (id)initWithDownloadItemCell:(DownloadItemCell*)cell
                      duration:(NSTimeInterval)duration
                animationCurve:(NSAnimationCurve)animationCurve;
@end

class BackgroundTheme : public ui::ThemeProvider {
public:
  BackgroundTheme(ui::ThemeProvider* provider);

  virtual void Init(Profile* profile) { }
  virtual SkBitmap* GetBitmapNamed(int id) const { return nil; }
  virtual SkColor GetColor(int id) const { return SkColor(); }
  virtual bool GetDisplayProperty(int id, int* result) const { return false; }
  virtual bool ShouldUseNativeFrame() const { return false; }
  virtual bool HasCustomImage(int id) const { return false; }
  virtual RefCountedMemory* GetRawData(int id) const { return NULL; }
  virtual NSImage* GetNSImageNamed(int id, bool allow_default) const;
  virtual NSColor* GetNSImageColorNamed(int id, bool allow_default) const;
  virtual NSColor* GetNSColor(int id, bool allow_default) const;
  virtual NSColor* GetNSColorTint(int id, bool allow_default) const;
  virtual NSGradient* GetNSGradient(int id) const;

private:
  ui::ThemeProvider* provider_;
  scoped_nsobject<NSGradient> buttonGradient_;
  scoped_nsobject<NSGradient> buttonPressedGradient_;
  scoped_nsobject<NSColor> borderColor_;
};

BackgroundTheme::BackgroundTheme(ui::ThemeProvider* provider) :
    provider_(provider) {
  NSColor* bgColor = [NSColor colorWithCalibratedRed:241/255.0
                                               green:245/255.0
                                                blue:250/255.0
                                               alpha:77/255.0];
  NSColor* clickedColor = [NSColor colorWithCalibratedRed:239/255.0
                                                    green:245/255.0
                                                     blue:252/255.0
                                                    alpha:51/255.0];

  borderColor_.reset(
      [[NSColor colorWithCalibratedWhite:0 alpha:36/255.0] retain]);
  buttonGradient_.reset([[NSGradient alloc]
      initWithColors:[NSArray arrayWithObject:bgColor]]);
  buttonPressedGradient_.reset([[NSGradient alloc]
      initWithColors:[NSArray arrayWithObject:clickedColor]]);
}

NSImage* BackgroundTheme::GetNSImageNamed(int id, bool allow_default) const {
  return nil;
}

NSColor* BackgroundTheme::GetNSImageColorNamed(int id,
                                               bool allow_default) const {
  return nil;
}

NSColor* BackgroundTheme::GetNSColor(int id, bool allow_default) const {
  return provider_->GetNSColor(id, allow_default);
}

NSColor* BackgroundTheme::GetNSColorTint(int id, bool allow_default) const {
  if (id == BrowserThemeProvider::TINT_BUTTONS)
    return borderColor_.get();

  return provider_->GetNSColorTint(id, allow_default);
}

NSGradient* BackgroundTheme::GetNSGradient(int id) const {
  switch (id) {
    case BrowserThemeProvider::GRADIENT_TOOLBAR_BUTTON:
    case BrowserThemeProvider::GRADIENT_TOOLBAR_BUTTON_INACTIVE:
      return buttonGradient_.get();
    case BrowserThemeProvider::GRADIENT_TOOLBAR_BUTTON_PRESSED:
    case BrowserThemeProvider::GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE:
      return buttonPressedGradient_.get();
    default:
      return provider_->GetNSGradient(id);
  }
}

@interface DownloadItemCell(Private)
- (void)updateTrackingAreas:(id)sender;
- (void)hideSecondaryTitle;
- (void)animation:(NSAnimation*)animation
       progressed:(NSAnimationProgress)progress;
- (NSString*)elideTitle:(int)availableWidth;
- (NSString*)elideStatus:(int)availableWidth;
- (ui::ThemeProvider*)backgroundThemeWrappingProvider:(ui::ThemeProvider*)provider;
- (BOOL)pressedWithDefaultThemeOnPart:(DownloadItemMousePosition)part;
- (NSColor*)titleColorForPart:(DownloadItemMousePosition)part;
- (void)drawSecondaryTitleInRect:(NSRect)innerFrame;
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
      [NSFont systemFontSizeForControlSize:NSSmallControlSize]]];

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

// For programmatic instantiations.
- (id)initTextCell:(NSString *)string {
  if ((self = [super initTextCell:string])) {
    [self setInitialState];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if ([completionAnimation_ isAnimating])
    [completionAnimation_ stopAnimation];
  if ([hideStatusAnimation_ isAnimating])
    [hideStatusAnimation_ stopAnimation];
  if (trackingAreaButton_) {
    [[self controlView] removeTrackingArea:trackingAreaButton_];
    trackingAreaButton_.reset();
  }
  if (trackingAreaDropdown_) {
    [[self controlView] removeTrackingArea:trackingAreaDropdown_];
    trackingAreaDropdown_.reset();
  }
  [secondaryTitle_ release];
  [secondaryFont_ release];
  [super dealloc];
}

- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel {
  // Set the name of the download.
  downloadPath_ = downloadModel->download()->GetFileNameToReportUser();

  string16 statusText = downloadModel->GetStatusText();
  if (statusText.empty()) {
    // Remove the status text label.
    [self hideSecondaryTitle];
    isStatusTextVisible_ = NO;
  } else {
    // Set status text.
    NSString* statusString = base::SysUTF16ToNSString(statusText);
    [self setSecondaryTitle:statusString];
    isStatusTextVisible_ = YES;
  }

  switch (downloadModel->download()->state()) {
    case DownloadItem::COMPLETE:
      // Small downloads may start in a complete state due to asynchronous
      // notifications. In this case, we'll get a second complete notification
      // via the observers, so we ignore it and avoid creating a second complete
      // animation.
      if (completionAnimation_.get())
        break;
      completionAnimation_.reset([[DownloadItemCellAnimation alloc]
          initWithDownloadItemCell:self
                          duration:kCompleteAnimationDuration
                    animationCurve:NSAnimationLinear]);
      [completionAnimation_.get() setDelegate:self];
      [completionAnimation_.get() startAnimation];
      percentDone_ = -1;
      break;
    case DownloadItem::CANCELLED:
      percentDone_ = -1;
      break;
    case DownloadItem::IN_PROGRESS:
      percentDone_ = downloadModel->download()->is_paused() ?
          -1 : downloadModel->download()->PercentComplete();
      break;
    default:
      NOTREACHED();
  }

  [[self controlView] setNeedsDisplay:YES];
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
  // The tracking areas are also used to decide how to handle clicks. They must
  // always be active, so the click is handled correctly when a download item
  // is clicked while chrome is not the active app ( http://crbug.com/21916 ).
  NSRect bounds = [[self controlView] bounds];
  NSRect buttonRect, dropdownRect;
  NSDivideRect(bounds, &dropdownRect, &buttonRect,
      kDropdownAreaWidth, NSMaxXEdge);

  trackingAreaButton_.reset([[NSTrackingArea alloc]
                  initWithRect:buttonRect
                       options:(NSTrackingMouseEnteredAndExited |
                                NSTrackingActiveAlways)
                         owner:self
                    userInfo:nil]);
  [[self controlView] addTrackingArea:trackingAreaButton_.get()];

  trackingAreaDropdown_.reset([[NSTrackingArea alloc]
                  initWithRect:dropdownRect
                       options:(NSTrackingMouseEnteredAndExited |
                                NSTrackingActiveAlways)
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
  return path;
}

- (NSString*)elideTitle:(int)availableWidth {
  NSFont* font = [self font];
  gfx::Font font_chr(base::SysNSStringToUTF16([font fontName]),
                     [font pointSize]);

  return base::SysUTF16ToNSString(
      ui::ElideFilename(downloadPath_, font_chr, availableWidth));
}

- (NSString*)elideStatus:(int)availableWidth {
  NSFont* font = [self secondaryFont];
  gfx::Font font_chr(base::SysNSStringToUTF16([font fontName]),
                     [font pointSize]);

  return base::SysUTF16ToNSString(ui::ElideText(
      base::SysNSStringToUTF16([self secondaryTitle]),
      font_chr,
      availableWidth,
      false));
}

- (ui::ThemeProvider*)backgroundThemeWrappingProvider:(ui::ThemeProvider*)provider {
  if (!themeProvider_.get()) {
    themeProvider_.reset(new BackgroundTheme(provider));
  }

  return themeProvider_.get();
}

// Returns if |part| was pressed while the default theme was active.
- (BOOL)pressedWithDefaultThemeOnPart:(DownloadItemMousePosition)part {
  ui::ThemeProvider* themeProvider =
      [[[self controlView] window] themeProvider];
  bool isDefaultTheme =
      !themeProvider->HasCustomImage(IDR_THEME_BUTTON_BACKGROUND);
  return isDefaultTheme && [self isHighlighted] && mousePosition_ == part;
}

// Returns the text color that should be used to draw text on |part|.
- (NSColor*)titleColorForPart:(DownloadItemMousePosition)part {
  ui::ThemeProvider* themeProvider =
      [[[self controlView] window] themeProvider];
  NSColor* themeTextColor =
      themeProvider->GetNSColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT,
                                true);
  return [self pressedWithDefaultThemeOnPart:part]
      ? [NSColor alternateSelectedControlTextColor] : themeTextColor;
}

- (void)drawSecondaryTitleInRect:(NSRect)innerFrame {
  if (![self secondaryTitle] || statusAlpha_ <= 0)
    return;

  CGFloat textWidth = innerFrame.size.width -
      (kTextPosLeft + kTextPaddingRight + kDropdownAreaWidth);
  NSString* secondaryText = [self elideStatus:textWidth];
  NSColor* secondaryColor =
      [self titleColorForPart:kDownloadItemMouseOverButtonPart];

  // If text is light-on-dark, lightening it alone will do nothing.
  // Therefore we mute luminance a wee bit before drawing in this case.
  if (![secondaryColor gtm_isDarkColor])
    secondaryColor = [secondaryColor gtm_colorByAdjustingLuminance:-0.2];

  NSDictionary* secondaryTextAttributes =
      [NSDictionary dictionaryWithObjectsAndKeys:
          secondaryColor, NSForegroundColorAttributeName,
          [self secondaryFont], NSFontAttributeName,
          nil];
  NSPoint secondaryPos =
      NSMakePoint(innerFrame.origin.x + kTextPosLeft, kSecondaryTextPosTop);
  [secondaryText drawAtPoint:secondaryPos
              withAttributes:secondaryTextAttributes];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // Constants from Cole.  Will kConstant them once the feedback loop
  // is complete.
  NSRect drawFrame = NSInsetRect(cellFrame, 1.5, 1.5);
  NSRect innerFrame = NSInsetRect(cellFrame, 2, 2);

  const float radius = 5;
  NSWindow* window = [controlView window];
  BOOL active = [window isKeyWindow] || [window isMainWindow];

  // In the default theme, draw download items with the bookmark button
  // gradient. For some themes, this leads to unreadable text, so draw the item
  // with a background that looks like windows (some transparent white) if a
  // theme is used. Use custom theme object with a white color gradient to trick
  // the superclass into drawing what we want.
  ui::ThemeProvider* themeProvider =
      [[[self controlView] window] themeProvider];
  bool isDefaultTheme =
      !themeProvider->HasCustomImage(IDR_THEME_BUTTON_BACKGROUND);

  NSGradient* bgGradient = nil;
  if (!isDefaultTheme) {
    themeProvider = [self backgroundThemeWrappingProvider:themeProvider];
    bgGradient = themeProvider->GetNSGradient(
        active ? BrowserThemeProvider::GRADIENT_TOOLBAR_BUTTON :
                 BrowserThemeProvider::GRADIENT_TOOLBAR_BUTTON_INACTIVE);
  }

  NSRect buttonDrawRect, dropdownDrawRect;
  NSDivideRect(drawFrame, &dropdownDrawRect, &buttonDrawRect,
      kDropdownAreaWidth, NSMaxXEdge);

  NSBezierPath* buttonInnerPath = [self
      leftRoundedPath:radius inRect:buttonDrawRect];
  NSBezierPath* dropdownInnerPath = [self
      rightRoundedPath:radius inRect:dropdownDrawRect];

  // Draw secondary title, if any. Do this before drawing the (transparent)
  // fill so that the text becomes a bit lighter. The default theme's "pressed"
  // gradient is not transparent, so only do this if a theme is active.
  bool drawStatusOnTop =
      [self pressedWithDefaultThemeOnPart:kDownloadItemMouseOverButtonPart];
  if (!drawStatusOnTop)
    [self drawSecondaryTitleInRect:innerFrame];

  // Stroke the borders and appropriate fill gradient.
  [self drawBorderAndFillForTheme:themeProvider
                      controlView:controlView
                        innerPath:buttonInnerPath
              showClickedGradient:[self isButtonPartPressed]
            showHighlightGradient:[self isMouseOverButtonPart]
                       hoverAlpha:0.0
                           active:active
                        cellFrame:cellFrame
                  defaultGradient:bgGradient];

  [self drawBorderAndFillForTheme:themeProvider
                      controlView:controlView
                        innerPath:dropdownInnerPath
              showClickedGradient:[self isDropdownPartPressed]
            showHighlightGradient:[self isMouseOverDropdownPart]
                       hoverAlpha:0.0
                           active:active
                        cellFrame:cellFrame
                  defaultGradient:bgGradient];

  [self drawInteriorWithFrame:innerFrame inView:controlView];

  // For the default theme, draw the status text on top of the (opaque) button
  // gradient.
  if (drawStatusOnTop)
    [self drawSecondaryTitleInRect:innerFrame];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // Draw title
  CGFloat textWidth = cellFrame.size.width -
      (kTextPosLeft + kTextPaddingRight + kDropdownAreaWidth);
  [self setTitle:[self elideTitle:textWidth]];

  NSColor* color = [self titleColorForPart:kDownloadItemMouseOverButtonPart];
  NSString* primaryText = [self title];

  NSDictionary* primaryTextAttributes =
      [NSDictionary dictionaryWithObjectsAndKeys:
          color, NSForegroundColorAttributeName,
          [self font], NSFontAttributeName,
          nil];
  NSPoint primaryPos = NSMakePoint(
      cellFrame.origin.x + kTextPosLeft,
      titleY_);

  [primaryText drawAtPoint:primaryPos withAttributes:primaryTextAttributes];

  // Draw progress disk
  {
    // CanvasSkiaPaint draws its content to the current NSGraphicsContext in its
    // destructor, which needs to be invoked before the icon is drawn below -
    // hence this nested block.

    // Always repaint the whole disk.
    NSPoint imagePosition = [self imageRectForBounds:cellFrame].origin;
    int x = imagePosition.x - download_util::kSmallProgressIconOffset;
    int y = imagePosition.y - download_util::kSmallProgressIconOffset;
    NSRect dirtyRect = NSMakeRect(
        x, y,
        download_util::kSmallProgressIconSize,
        download_util::kSmallProgressIconSize);

    gfx::CanvasSkiaPaint canvas(dirtyRect, false);
    canvas.set_composite_alpha(true);
    if (completionAnimation_.get()) {
      if ([completionAnimation_ isAnimating]) {
        download_util::PaintDownloadComplete(&canvas,
            x, y,
            [completionAnimation_ currentValue],
            download_util::SMALL);
      }
    } else if (percentDone_ >= 0) {
      download_util::PaintDownloadProgress(&canvas,
          x, y,
          download_util::kStartAngleDegrees,  // TODO(thakis): Animate
          percentDone_,
          download_util::SMALL);
    }
  }

  // Draw icon
  NSRect imageRect = NSZeroRect;
  imageRect.size = [[self image] size];
  [[self image] drawInRect:[self imageRectForBounds:cellFrame]
                  fromRect:imageRect
                 operation:NSCompositeSourceOver
                  fraction:[self isEnabled] ? 1.0 : 0.5
              neverFlipped:YES];

  // Separator between button and popup parts
  CGFloat lx = NSMaxX(cellFrame) - kDropdownAreaWidth + 0.5;
  [[NSColor colorWithDeviceWhite:0.0 alpha:0.1] set];
  [NSBezierPath strokeLineFromPoint:NSMakePoint(lx, NSMinY(cellFrame) + 1)
                            toPoint:NSMakePoint(lx, NSMaxY(cellFrame) - 1)];
  [[NSColor colorWithDeviceWhite:1.0 alpha:0.1] set];
  [NSBezierPath strokeLineFromPoint:NSMakePoint(lx + 1, NSMinY(cellFrame) + 1)
                            toPoint:NSMakePoint(lx + 1, NSMaxY(cellFrame) - 1)];

  // Popup arrow. Put center of mass of the arrow in the center of the
  // dropdown area.
  CGFloat cx = NSMaxX(cellFrame) - kDropdownAreaWidth/2 + 0.5;
  CGFloat cy = NSMidY(cellFrame);
  NSPoint p1 = NSMakePoint(cx - kDropdownArrowWidth/2,
                           cy - kDropdownArrowHeight/3 + kDropdownAreaY);
  NSPoint p2 = NSMakePoint(cx + kDropdownArrowWidth/2,
                           cy - kDropdownArrowHeight/3 + kDropdownAreaY);
  NSPoint p3 = NSMakePoint(cx, cy + kDropdownArrowHeight*2/3 + kDropdownAreaY);
  NSBezierPath *triangle = [NSBezierPath bezierPath];
  [triangle moveToPoint:p1];
  [triangle lineToPoint:p2];
  [triangle lineToPoint:p3];
  [triangle closePath];

  NSGraphicsContext* context = [NSGraphicsContext currentContext];
  [context saveGraphicsState];

  scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow.get() setShadowColor:[NSColor whiteColor]];
  [shadow.get() setShadowOffset:NSMakeSize(0, -1)];
  [shadow setShadowBlurRadius:0.0];
  [shadow set];

  NSColor* fill = [self titleColorForPart:kDownloadItemMouseOverDropdownPart];
  [fill setFill];

  [triangle fill];

  [context restoreGraphicsState];
}

- (NSRect)imageRectForBounds:(NSRect)cellFrame {
  return NSMakeRect(cellFrame.origin.x + kImagePaddingLeft,
                    cellFrame.origin.y + kImagePaddingTop,
                    kImageWidth,
                    kImageHeight);
}

- (void)hideSecondaryTitle {
  if (isStatusTextVisible_) {
    // No core animation -- text in CA layers is not subpixel antialiased :-/
    hideStatusAnimation_.reset([[DownloadItemCellAnimation alloc]
        initWithDownloadItemCell:self
                        duration:kHideStatusDuration
                  animationCurve:NSAnimationEaseIn]);
    [hideStatusAnimation_.get() setDelegate:self];
    [hideStatusAnimation_.get() startAnimation];
  } else {
    // If the download is done so quickly that the status line is never visible,
    // don't show an animation
    [self animation:nil progressed:1.0];
  }
}

- (void)animation:(NSAnimation*)animation
      progressed:(NSAnimationProgress)progress {
  if (animation == hideStatusAnimation_ || animation == nil) {
    titleY_ = progress*kPrimaryTextOnlyPosTop +
        (1 - progress)*kPrimaryTextPosTop;
    statusAlpha_ = 1 - progress;
    [[self controlView] setNeedsDisplay:YES];
  } else if (animation == completionAnimation_) {
    [[self controlView] setNeedsDisplay:YES];
  }
}

- (void)animationDidEnd:(NSAnimation *)animation {
  if (animation == hideStatusAnimation_)
    hideStatusAnimation_.reset();
  else if (animation == completionAnimation_)
    completionAnimation_.reset();
}

@end

@implementation DownloadItemCellAnimation

- (id)initWithDownloadItemCell:(DownloadItemCell*)cell
                      duration:(NSTimeInterval)duration
                animationCurve:(NSAnimationCurve)animationCurve {
  if ((self = [super gtm_initWithDuration:duration
                                eventMask:NSLeftMouseDownMask
                           animationCurve:animationCurve])) {
    cell_ = cell;
    [self setAnimationBlockingMode:NSAnimationNonblocking];
  }
  return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  [super setCurrentProgress:progress];
  [cell_ animation:self progressed:progress];
}

@end

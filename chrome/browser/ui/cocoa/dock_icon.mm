// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dock_icon.h"

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_nsobject.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

using content::BrowserThread;

namespace {

// The fraction of the size of the dock icon that the badge is.
const float kBadgeFraction = 0.4f;

// The indentation of the badge.
const float kBadgeIndent = 5.0f;

// The maximum update rate for the dock icon. 200ms = 5fps.
const int64 kUpdateFrequencyMs = 200;

}  // namespace

// A view that draws our dock tile.
@interface DockTileView : NSView {
 @private
  int downloads_;
  BOOL indeterminate_;
  float progress_;
}

// Indicates how many downloads are in progress.
@property (nonatomic) int downloads;

// Indicates whether the progress indicator should be in an indeterminate state
// or not.
@property (nonatomic) BOOL indeterminate;

// Indicates the amount of progress made of the download. Ranges from [0..1].
@property (nonatomic) float progress;

@end

@implementation DockTileView

@synthesize downloads = downloads_;
@synthesize indeterminate = indeterminate_;
@synthesize progress = progress_;

- (void)drawRect:(NSRect)dirtyRect {
  // Not -[NSApplication applicationIconImage]; that fails to return a pasted
  // custom icon.
  NSString* appPath = [base::mac::MainBundle() bundlePath];
  NSImage* appIcon = [[NSWorkspace sharedWorkspace] iconForFile:appPath];
  [appIcon drawInRect:[self bounds]
             fromRect:NSZeroRect
            operation:NSCompositeSourceOver
             fraction:1.0];

  if (downloads_ == 0)
    return;

  NSRect badgeRect = [self bounds];
  badgeRect.size.height = (int)(kBadgeFraction * badgeRect.size.height);
  int newWidth = kBadgeFraction * NSWidth(badgeRect);
  badgeRect.origin.x = NSWidth(badgeRect) - newWidth;
  badgeRect.size.width = newWidth;

  CGFloat badgeRadius = NSMidY(badgeRect);

  badgeRect.origin.x -= kBadgeIndent;
  badgeRect.origin.y += kBadgeIndent;

  NSPoint badgeCenter = NSMakePoint(NSMidX(badgeRect), NSMidY(badgeRect));

  // Background
  NSColor* backgroundColor = [NSColor colorWithCalibratedRed:0.85
                                                       green:0.85
                                                        blue:0.85
                                                       alpha:1.0];
  NSColor* backgroundHighlight =
      [backgroundColor blendedColorWithFraction:0.85
                                        ofColor:[NSColor whiteColor]];
  base::scoped_nsobject<NSGradient> backgroundGradient(
      [[NSGradient alloc] initWithStartingColor:backgroundHighlight
                                    endingColor:backgroundColor]);
  NSBezierPath* badgeEdge = [NSBezierPath bezierPathWithOvalInRect:badgeRect];
  {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    [badgeEdge addClip];
    [backgroundGradient drawFromCenter:badgeCenter
                                radius:0.0
                              toCenter:badgeCenter
                                radius:badgeRadius
                               options:0];
  }

  // Slice
  if (!indeterminate_) {
    NSColor* sliceColor = [NSColor colorWithCalibratedRed:0.45
                                                    green:0.8
                                                     blue:0.25
                                                    alpha:1.0];
    NSColor* sliceHighlight =
        [sliceColor blendedColorWithFraction:0.4
                                     ofColor:[NSColor whiteColor]];
    base::scoped_nsobject<NSGradient> sliceGradient(
        [[NSGradient alloc] initWithStartingColor:sliceHighlight
                                      endingColor:sliceColor]);
    NSBezierPath* progressSlice;
    if (progress_ >= 1.0) {
      progressSlice = [NSBezierPath bezierPathWithOvalInRect:badgeRect];
    } else {
      CGFloat endAngle = 90.0 - 360.0 * progress_;
      if (endAngle < 0.0)
        endAngle += 360.0;
      progressSlice = [NSBezierPath bezierPath];
      [progressSlice moveToPoint:badgeCenter];
      [progressSlice appendBezierPathWithArcWithCenter:badgeCenter
                                                radius:badgeRadius
                                            startAngle:90.0
                                              endAngle:endAngle
                                             clockwise:YES];
      [progressSlice closePath];
    }
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    [progressSlice addClip];
    [sliceGradient drawFromCenter:badgeCenter
                           radius:0.0
                         toCenter:badgeCenter
                           radius:badgeRadius
                          options:0];
  }

  // Edge
  {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    [[NSColor whiteColor] set];
    base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
    [shadow.get() setShadowOffset:NSMakeSize(0, -2)];
    [shadow setShadowBlurRadius:2];
    [shadow set];
    [badgeEdge setLineWidth:2];
    [badgeEdge stroke];
  }

  // Download count
  base::scoped_nsobject<NSNumberFormatter> formatter(
      [[NSNumberFormatter alloc] init]);
  NSString* countString =
      [formatter stringFromNumber:[NSNumber numberWithInt:downloads_]];

  base::scoped_nsobject<NSShadow> countShadow([[NSShadow alloc] init]);
  [countShadow setShadowBlurRadius:3.0];
  [countShadow.get() setShadowColor:[NSColor whiteColor]];
  [countShadow.get() setShadowOffset:NSMakeSize(0.0, 0.0)];
  NSMutableDictionary* countAttrsDict =
      [NSMutableDictionary dictionaryWithObjectsAndKeys:
          [NSColor blackColor], NSForegroundColorAttributeName,
          countShadow.get(), NSShadowAttributeName,
          nil];
  CGFloat countFontSize = badgeRadius;
  NSSize countSize = NSZeroSize;
  base::scoped_nsobject<NSAttributedString> countAttrString;
  while (1) {
    NSFont* countFont = [NSFont fontWithName:@"Helvetica-Bold"
                                        size:countFontSize];

    // This will generally be plain Helvetica.
    if (!countFont)
      countFont = [NSFont userFontOfSize:countFontSize];

    // Continued failure would generate an NSException.
    if (!countFont)
      break;

    [countAttrsDict setObject:countFont forKey:NSFontAttributeName];
    countAttrString.reset(
        [[NSAttributedString alloc] initWithString:countString
                                        attributes:countAttrsDict]);
    countSize = [countAttrString size];
    if (countSize.width > badgeRadius * 1.5) {
      countFontSize -= 1.0;
    } else {
      break;
    }
  }

  NSPoint countOrigin = badgeCenter;
  countOrigin.x -= countSize.width / 2;
  countOrigin.y -= countSize.height / 2.2;  // tweak; otherwise too low

  [countAttrString.get() drawAtPoint:countOrigin];
}

@end


@implementation DockIcon

+ (DockIcon*)sharedDockIcon {
  static DockIcon* icon;
  if (!icon) {
    NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];

    base::scoped_nsobject<DockTileView> dockTileView(
        [[DockTileView alloc] init]);
    [dockTile setContentView:dockTileView];

    icon = [[DockIcon alloc] init];
  }

  return icon;
}

- (void)updateIcon {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::TimeDelta updateFrequency =
      base::TimeDelta::FromMilliseconds(kUpdateFrequencyMs);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta timeSinceLastUpdate = now - lastUpdate_;
  if (!forceUpdate_ && timeSinceLastUpdate < updateFrequency)
    return;

  lastUpdate_ = now;
  forceUpdate_ = NO;

  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];

  [dockTile display];
}

- (void)setDownloads:(int)downloads {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];
  DockTileView* dockTileView = (DockTileView*)([dockTile contentView]);

  if (downloads != [dockTileView downloads]) {
    [dockTileView setDownloads:downloads];
    forceUpdate_ = YES;
  }
}

- (void)setIndeterminate:(BOOL)indeterminate {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];
  DockTileView* dockTileView = (DockTileView*)([dockTile contentView]);

  if (indeterminate != [dockTileView indeterminate]) {
    [dockTileView setIndeterminate:indeterminate];
    forceUpdate_ = YES;
  }
}

- (void)setProgress:(float)progress {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];
  DockTileView* dockTileView = (DockTileView*)([dockTile contentView]);

  [dockTileView setProgress:progress];
}

@end

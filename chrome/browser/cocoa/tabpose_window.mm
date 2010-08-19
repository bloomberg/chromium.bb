// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tabpose_window.h"

#import <QuartzCore/QuartzCore.h>

#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/app_resources.h"
#include "skia/ext/skia_utils_mac.h"

const int kTopGradientHeight  = 15;

NSString* const kAnimationIdKey = @"AnimationId";
NSString* const kAnimationIdFadeIn = @"FadeIn";
NSString* const kAnimationIdFadeOut = @"FadeOut";

const CGFloat kDefaultAnimationDuration = 0.25;  // In seconds.
const CGFloat kSlomoFactor = 4;

// CAGradientLayer is 10.6-only -- roll our own.
@interface DarkGradientLayer : CALayer
- (void)drawInContext:(CGContextRef)context;
@end

@implementation DarkGradientLayer
- (void)drawInContext:(CGContextRef)context {
  scoped_cftyperef<CGColorSpaceRef> grayColorSpace(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericGray));
  CGFloat grays[] = { 0.277, 1.0, 0.39, 1.0 };
  CGFloat locations[] = { 0, 1 };
  scoped_cftyperef<CGGradientRef> gradient(CGGradientCreateWithColorComponents(
      grayColorSpace.get(), grays, locations, arraysize(locations)));
  CGPoint topLeft = CGPointMake(0.0, kTopGradientHeight);
  CGContextDrawLinearGradient(context, gradient.get(), topLeft, CGPointZero, 0);
}
@end

namespace {

class ScopedCAActionDisabler {
 public:
  ScopedCAActionDisabler() {
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithBool:YES]
                     forKey:kCATransactionDisableActions];
  }

  ~ScopedCAActionDisabler() {
    [CATransaction commit];
  }
};

class ScopedCAActionSetDuration {
 public:
  explicit ScopedCAActionSetDuration(CGFloat duration) {
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithFloat:duration]
                     forKey:kCATransactionAnimationDuration];
  }

  ~ScopedCAActionSetDuration() {
    [CATransaction commit];
  }
};

}  // namespace

// Given the number |n| of tiles with a desired aspect ratio of |a| and a
// desired distance |dx|, |dy| between tiles, returns how many tiles fit
// vertically into a rectangle with the dimensions |w_c|, |h_c|. This returns
// an exact solution, which is usually a fractional number.
static float FitNRectsWithAspectIntoBoundingSizeWithConstantPadding(
    int n, double a, int w_c, int h_c, int dx, int dy) {
  // We want to have the small rects have the same aspect ratio a as a full
  // tab. Let w, h be the size of a small rect, and w_c, h_c the size of the
  // container. dx, dy are the distances between small rects in x, y direction.

  // Geometry yields:
  // w_c = nx * (w + dx) - dx <=> w = (w_c + d_x) / nx - d_x
  // h_c = ny * (h + dy) - dy <=> h = (h_c + d_y) / ny - d_t
  // Plugging this into
  // a := tab_width / tab_height = w / h
  // yields
  // a = ((w_c - (nx - 1)*d_x)*ny) / (nx*(h_c - (ny - 1)*d_y))
  // Plugging in nx = n/ny and pen and paper (or wolfram alpha:
  // http://www.wolframalpha.com/input/?i=(-sqrt((d+n-a+f+n)^2-4+(a+f%2Ba+h)+(-d+n-n+w))%2Ba+f+n-d+n)/(2+a+(f%2Bh)) , (solution for nx)
  // http://www.wolframalpha.com/input/?i=+(-sqrt((a+f+n-d+n)^2-4+(d%2Bw)+(-a+f+n-a+h+n))-a+f+n%2Bd+n)/(2+(d%2Bw)) , (solution for ny)
  // ) gives us nx and ny (but the wrong root -- s/-sqrt(FOO)/sqrt(FOO)/.

  // This function returns ny.
  return (sqrt(pow(n * (a * dy - dx), 2) +
               4 * n * a * (dx + w_c) * (dy + h_c)) -
          n * (a * dy - dx))
      /
         (2 * (dx + w_c));
}

namespace tabpose {

// A tile is what is shown for a single tab in tabpose mode. It consists of a
// title, favicon, thumbnail image, and pre- and postanimation rects.
// TODO(thakis): Right now, it only consists of a thumb rect.
class Tile {
 public:
  // Returns the rectangle this thumbnail is at at the beginning of the zoom-in
  // animation. |tile| is the rectangle that's covering the whole tab area when
  // the animation starts.
  NSRect GetStartRectRelativeTo(const Tile& tile) const;
  NSRect thumb_rect() const { return thumb_rect_; }

  NSRect favicon_rect() const { return favicon_rect_; }
  SkBitmap favicon() const;

  // This changes |title_rect| and |favicon_rect| such that the favicon is on
  // the font's baseline and that the minimum distance between thumb rect and
  // favicon and title rects doesn't change.
  // The view code
  // 1. queries desired font size by calling |title_font_size()|
  // 2. loads that font
  // 3. calls |set_font_metrics()| which updates the title rect
  // 4. receives the title rect and puts the title on it with the font from 2.
  void set_font_metrics(CGFloat ascender, CGFloat descender);
  CGFloat title_font_size() const { return title_font_size_; }

  NSRect title_rect() const { return title_rect_; }

  // Returns an unelided title. The view logic is responsible for eliding.
  const string16& title() const { return contents_->GetTitle(); }
 private:
  friend class TileSet;

  // The thumb rect includes infobars, detached thumbnail bar, web contents,
  // and devtools.
  NSRect thumb_rect_;
  NSRect start_thumb_rect_;

  NSRect favicon_rect_;

  CGFloat title_font_size_;
  NSRect title_rect_;

  TabContents* contents_;  // weak
};

NSRect Tile::GetStartRectRelativeTo(const Tile& tile) const {
  NSRect rect = start_thumb_rect_;
  rect.origin.x -= tile.start_thumb_rect_.origin.x;
  rect.origin.y -= tile.start_thumb_rect_.origin.y;
  return rect;
}

SkBitmap Tile::favicon() const {
  if (contents_->is_app()) {
    SkBitmap* icon = contents_->GetExtensionAppIcon();
    if (icon)
      return *icon;
  }
  return contents_->GetFavIcon();
}

// Changes |title_rect| and |favicon_rect| such that the favicon is on the
// font's baseline and that the minimum distance between thumb rect and
// favicon and title rects doesn't change.
void Tile::set_font_metrics(CGFloat ascender, CGFloat descender) {
  title_rect_.origin.y -= ascender + descender - NSHeight(title_rect_);
  title_rect_.size.height = ascender + descender;

  if (NSHeight(favicon_rect_) < ascender) {
    // Move favicon down.
    favicon_rect_.origin.y = title_rect_.origin.y + descender;
  } else {
    // Move title down.
    title_rect_.origin.y = favicon_rect_.origin.y - descender;
  }
}

// A tileset is responsible for owning and laying out all |Tile|s shown in a
// tabpose window.
class TileSet {
 public:
  // Fills in |tiles_|.
  void Build(TabStripModel* source_model);

  // Computes coordinates for |tiles_|.
  void Layout(NSRect containing_rect);

  int selected_index() const { return selected_index_; }
  void set_selected_index(int index);
  void ResetSelectedIndex() { selected_index_ = initial_index_; }

  const Tile& selected_tile() const { return *tiles_[selected_index()]; }
  Tile& tile_at(int index) { return *tiles_[index]; }
  const Tile& tile_at(int index) const { return *tiles_[index]; }

 private:
  ScopedVector<Tile> tiles_;

  int selected_index_;
  int initial_index_;
};

void TileSet::Build(TabStripModel* source_model) {
  selected_index_ = initial_index_ = source_model->selected_index();
  tiles_.resize(source_model->count());
  for (size_t i = 0; i < tiles_.size(); ++i) {
    tiles_[i] = new Tile;
    tiles_[i]->contents_ = source_model->GetTabContentsAt(i);
  }
}

void TileSet::Layout(NSRect containing_rect) {
  int tile_count = tiles_.size();

  // Room around the tiles insde of |containing_rect|.
  const int kSmallPaddingTop = 30;
  const int kSmallPaddingLeft = 30;
  const int kSmallPaddingRight = 30;
  const int kSmallPaddingBottom = 30;

  // Favicon / title area.
  const int kThumbTitlePaddingY = 6;
  const int kFaviconSize = 16;
  const int kTitleHeight = 14;  // Font size.
  const int kTitleExtraHeight = kThumbTitlePaddingY + kTitleHeight;
  const int kFaviconExtraHeight = kThumbTitlePaddingY + kFaviconSize;
  const int kFaviconTitleDistanceX = 6;
  const int kFooterExtraHeight =
      std::max(kFaviconExtraHeight, kTitleExtraHeight);

  // Room between the tiles.
  const int kSmallPaddingX = 15;
  const int kSmallPaddingY = kFooterExtraHeight;

  // Aspect ratio of the containing rect.
  CGFloat aspect = NSWidth(containing_rect) / NSHeight(containing_rect);

  // Room left in container after the outer padding is removed.
  double container_width =
      NSWidth(containing_rect) - kSmallPaddingLeft - kSmallPaddingRight;
  double container_height =
      NSHeight(containing_rect) - kSmallPaddingTop - kSmallPaddingBottom;

  // The tricky part is figuring out the size of a tab thumbnail, or since the
  // size of the containing rect is known, the number of tiles in x and y
  // direction.
  // Given are the size of the containing rect, and the number of thumbnails
  // that need to fit into that rect. The aspect ratio of the thumbnails needs
  // to be the same as that of |containing_rect|, else they will look distorted.
  // The thumbnails need to be distributed such that
  // |count_x * count_y >= tile_count|, and such that wasted space is minimized.
  //  See the comments in
  // |FitNRectsWithAspectIntoBoundingSizeWithConstantPadding()| for a more
  // detailed discussion.
  // TODO(thakis): It might be good enough to choose |count_x| and |count_y|
  //   such that count_x / count_y is roughly equal to |aspect|?
  double fny = FitNRectsWithAspectIntoBoundingSizeWithConstantPadding(
      tile_count, aspect,
      container_width, container_height - kFooterExtraHeight,
      kSmallPaddingX, kSmallPaddingY + kFooterExtraHeight);
  int count_y(roundf(fny));
  int count_x(ceilf(tile_count / float(count_y)));
  int last_row_count_x = tile_count - count_x * (count_y - 1);

  // Now that |count_x| and |count_y| are known, it's straightforward to compute
  // thumbnail width/height. See comment in
  // |FitNRectsWithAspectIntoBoundingSizeWithConstantPadding| for the derivation
  // of these two formulas.
  int small_width =
      floor((container_width + kSmallPaddingX) / float(count_x) -
            kSmallPaddingX);
  int small_height =
      floor((container_height + kSmallPaddingY) / float(count_y) -
            (kSmallPaddingY + kFooterExtraHeight));

  // |small_width / small_height| has only roughly an aspect ratio of |aspect|.
  // Shrink the thumbnail rect to make the aspect ratio fit exactly, and add
  // the extra space won by shrinking to the outer padding.
  int smallExtraPaddingLeft = 0;
  int smallExtraPaddingTop = 0;
  if (aspect > small_width/float(small_height)) {
    small_height = small_width / aspect;
    CGFloat all_tiles_height =
        (small_height + kSmallPaddingY + kFooterExtraHeight) * count_y -
        (kSmallPaddingY + kFooterExtraHeight);
    smallExtraPaddingTop = (container_height - all_tiles_height)/2;
  } else {
    small_width = small_height * aspect;
    CGFloat all_tiles_width =
        (small_width + kSmallPaddingX) * count_x - kSmallPaddingX;
    smallExtraPaddingLeft = (container_width - all_tiles_width)/2;
  }

  // Compute inter-tile padding in the zoomed-out view.
  CGFloat scale_small_to_big = NSWidth(containing_rect) / float(small_width);
  CGFloat big_padding_x = kSmallPaddingX * scale_small_to_big;
  CGFloat big_padding_y =
      (kSmallPaddingY + kFooterExtraHeight) * scale_small_to_big;

  // Now all dimensions are known. Lay out all tiles on a regular grid:
  // X X X X
  // X X X X
  // X X
  for (int row = 0, i = 0; i < tile_count; ++row) {
    for (int col = 0; col < count_x && i < tile_count; ++col, ++i) {
      // Compute the smalled, zoomed-out thumbnail rect.
      tiles_[i]->thumb_rect_.size = NSMakeSize(small_width, small_height);

      int small_x = col * (small_width + kSmallPaddingX) +
                    kSmallPaddingLeft + smallExtraPaddingLeft;
      int small_y = row * (small_height + kSmallPaddingY + kFooterExtraHeight) +
                    kSmallPaddingTop + smallExtraPaddingTop;

      tiles_[i]->thumb_rect_.origin = NSMakePoint(
          small_x, NSHeight(containing_rect) - small_y - small_height);

      tiles_[i]->favicon_rect_.size = NSMakeSize(kFaviconSize, kFaviconSize);
      tiles_[i]->favicon_rect_.origin = NSMakePoint(
          small_x,
          NSHeight(containing_rect) -
              (small_y + small_height + kFaviconExtraHeight));

      // Align lower left corner of title rect with lower left corner of favicon
      // for now. The final position is computed later by
      // |Tile::set_font_metrics()|.
      tiles_[i]->title_font_size_ = kTitleHeight;
      tiles_[i]->title_rect_.origin = NSMakePoint(
          NSMaxX(tiles_[i]->favicon_rect()) + kFaviconTitleDistanceX,
          NSMinY(tiles_[i]->favicon_rect()));
      tiles_[i]->title_rect_.size = NSMakeSize(
          small_width -
              NSWidth(tiles_[i]->favicon_rect()) - kFaviconTitleDistanceX,
          kTitleHeight);

      // Compute the big, pre-zoom thumbnail rect.
      tiles_[i]->start_thumb_rect_.size = containing_rect.size;

      int big_x = col * (NSWidth(containing_rect) + big_padding_x);
      int big_y = row * (NSHeight(containing_rect) + big_padding_y);
      tiles_[i]->start_thumb_rect_.origin = NSMakePoint(big_x, -big_y);
    }
  }

  // Go through last row and center it:
  // X X X X
  // X X X X
  //   X X
  int last_row_empty_tiles_x = count_x - last_row_count_x;
  CGFloat small_last_row_shift_x =
      last_row_empty_tiles_x * (small_width + kSmallPaddingX) / 2;
  CGFloat big_last_row_shift_x =
      last_row_empty_tiles_x * (NSWidth(containing_rect) + big_padding_x) / 2;
  for (int i = tile_count - last_row_count_x; i < tile_count; ++i) {
    tiles_[i]->thumb_rect_.origin.x += small_last_row_shift_x;
    tiles_[i]->start_thumb_rect_.origin.x += big_last_row_shift_x;
    tiles_[i]->favicon_rect_.origin.x += small_last_row_shift_x;
    tiles_[i]->title_rect_.origin.x += small_last_row_shift_x;
  }
}

void TileSet::set_selected_index(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, static_cast<int>(tiles_.size()));
  selected_index_ = index;
}

}  // namespace tabpose

void AnimateCALayerFrameFromTo(
    CALayer* layer, const NSRect& from, const NSRect& to,
    NSTimeInterval duration, id boundsAnimationDelegate) {
  // http://developer.apple.com/mac/library/qa/qa2008/qa1620.html
  CABasicAnimation* animation;

  animation = [CABasicAnimation animationWithKeyPath:@"bounds"];
  animation.fromValue = [NSValue valueWithRect:from];
  animation.toValue = [NSValue valueWithRect:to];
  animation.duration = duration;
  animation.timingFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
  animation.delegate = boundsAnimationDelegate;

  // Update the layer's bounds so the layer doesn't snap back when the animation
  // completes.
  layer.bounds = NSRectToCGRect(to);

  // Add the animation, overriding the implicit animation.
  [layer addAnimation:animation forKey:@"bounds"];

  // Prepare the animation from the current position to the new position.
  NSPoint opoint = from.origin;
  NSPoint point = to.origin;

  // Adapt to anchorPoint.
  opoint.x += NSWidth(from) * layer.anchorPoint.x;
  opoint.y += NSHeight(from) * layer.anchorPoint.y;
  point.x += NSWidth(to) * layer.anchorPoint.x;
  point.y += NSHeight(to) * layer.anchorPoint.y;

  animation = [CABasicAnimation animationWithKeyPath:@"position"];
  animation.fromValue = [NSValue valueWithPoint:opoint];
  animation.toValue = [NSValue valueWithPoint:point];
  animation.duration = duration;
  animation.timingFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];

  // Update the layer's position so that the layer doesn't snap back when the
  // animation completes.
  layer.position = NSPointToCGPoint(point);

  // Add the animation, overriding the implicit animation.
  [layer addAnimation:animation forKey:@"position"];
}

@interface TabposeWindow (Private)
- (id)initForWindow:(NSWindow*)parent
               rect:(NSRect)rect
              slomo:(BOOL)slomo
      tabStripModel:(TabStripModel*)tabStripModel;
- (void)setUpLayers:(NSRect)bgLayerRect slomo:(BOOL)slomo;
- (void)fadeAway:(BOOL)slomo;
- (void)selectTileAtIndex:(int)newIndex;
@end

@implementation TabposeWindow

+ (id)openTabposeFor:(NSWindow*)parent
                rect:(NSRect)rect
               slomo:(BOOL)slomo
       tabStripModel:(TabStripModel*)tabStripModel {
  // Releases itself when closed.
  return [[TabposeWindow alloc]
      initForWindow:parent rect:rect slomo:slomo tabStripModel:tabStripModel];
}

- (id)initForWindow:(NSWindow*)parent
               rect:(NSRect)rect
              slomo:(BOOL)slomo
      tabStripModel:(TabStripModel*)tabStripModel {
  NSRect frame = [parent frame];
  if ((self = [super initWithContentRect:frame
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    // TODO(thakis): Add a TabStripModelObserver to |tabStripModel_|.
    tabStripModel_ = tabStripModel;
    state_ = tabpose::kFadingIn;
    tileSet_.reset(new tabpose::TileSet);
    [self setReleasedWhenClosed:YES];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setUpLayers:rect slomo:slomo];
    [self setAcceptsMouseMovedEvents:YES];
    [parent addChildWindow:self ordered:NSWindowAbove];
    [self makeKeyAndOrderFront:self];
  }
  return self;
}

- (CALayer*)selectedLayer {
  return [allThumbnailLayers_ objectAtIndex:tileSet_->selected_index()];
}

- (void)selectTileAtIndex:(int)newIndex {
  ScopedCAActionDisabler disabler;
  const tabpose::Tile& tile = tileSet_->tile_at(newIndex);
  selectionHighlight_.frame =
      NSRectToCGRect(NSInsetRect(tile.thumb_rect(), -5, -5));

  tileSet_->set_selected_index(newIndex);
}

- (void)setUpLayers:(NSRect)bgLayerRect slomo:(BOOL)slomo {
  // Root layer -- covers whole window.
  rootLayer_ = [CALayer layer];
  [[self contentView] setLayer:rootLayer_];
  [[self contentView] setWantsLayer:YES];

  // Background layer -- the visible part of the window.
  gray_.reset(CGColorCreateGenericGray(0.39, 1.0));
  bgLayer_ = [CALayer layer];
  bgLayer_.backgroundColor = gray_;
  bgLayer_.frame = NSRectToCGRect(bgLayerRect);
  bgLayer_.masksToBounds = YES;
  [rootLayer_ addSublayer:bgLayer_];

  // Selection highlight layer.
  darkBlue_.reset(CGColorCreateGenericRGB(0.25, 0.34, 0.86, 1.0));
  selectionHighlight_ = [CALayer layer];
  selectionHighlight_.backgroundColor = darkBlue_;
  selectionHighlight_.cornerRadius = 5.0;
  selectionHighlight_.zPosition = -1;  // Behind other layers.
  selectionHighlight_.hidden = YES;
  [bgLayer_ addSublayer:selectionHighlight_];

  // Top gradient.
  CALayer* gradientLayer = [DarkGradientLayer layer];
  gradientLayer.frame = CGRectMake(
      0,
      NSHeight(bgLayerRect) - kTopGradientHeight,
      NSWidth(bgLayerRect),
      kTopGradientHeight);
  [gradientLayer setNeedsDisplay];  // Draw once.
  [bgLayer_ addSublayer:gradientLayer];

  // Layers for the tab thumbnails.
  tileSet_->Build(tabStripModel_);
  tileSet_->Layout(bgLayerRect);

  NSImage* defaultFavIcon = ResourceBundle::GetSharedInstance().GetNSImageNamed(
      IDR_DEFAULT_FAVICON);

  allThumbnailLayers_.reset(
      [[NSMutableArray alloc] initWithCapacity:tabStripModel_->count()]);
  allFaviconLayers_.reset(
      [[NSMutableArray alloc] initWithCapacity:tabStripModel_->count()]);
  allTitleLayers_.reset(
      [[NSMutableArray alloc] initWithCapacity:tabStripModel_->count()]);
  for (int i = 0; i < tabStripModel_->count(); ++i) {
    const tabpose::Tile& tile = tileSet_->tile_at(i);
    CALayer* layer = [CALayer layer];

    // Background color as placeholder for now.
    layer.backgroundColor = CGColorGetConstantColor(kCGColorWhite);

    AnimateCALayerFrameFromTo(
        layer,
        tile.GetStartRectRelativeTo(tileSet_->selected_tile()),
        tile.thumb_rect(),
        kDefaultAnimationDuration * (slomo ? kSlomoFactor : 1),
        i == tileSet_->selected_index() ? self : nil);

    // Add a delegate to one of the animations to get a notification once the
    // animations are done.
    if (i == tileSet_->selected_index()) {
      CAAnimation* animation = [layer animationForKey:@"bounds"];
      DCHECK(animation);
      [animation setValue:kAnimationIdFadeIn forKey:kAnimationIdKey];
    }

    layer.shadowRadius = 10;
    layer.shadowOffset = CGSizeMake(0, -10);

    [bgLayer_ addSublayer:layer];
    [allThumbnailLayers_ addObject:layer];

    // Favicon and title.
    NSFont* font = [NSFont systemFontOfSize:tile.title_font_size()];
    tileSet_->tile_at(i).set_font_metrics([font ascender], -[font descender]);

    NSImage* ns_favicon = gfx::SkBitmapToNSImage(tile.favicon());
    // Either we don't have a valid favicon or there was some issue converting
    // it from an SkBitmap. Either way, just show the default.
    if (!ns_favicon)
      ns_favicon = defaultFavIcon;
    scoped_cftyperef<CGImageRef> favicon(
        mac_util::CopyNSImageToCGImage(ns_favicon));

    CALayer* faviconLayer = [CALayer layer];
    faviconLayer.frame = NSRectToCGRect(tile.favicon_rect());
    faviconLayer.contents = (id)favicon.get();
    faviconLayer.zPosition = 1;  // On top of the thumb shadow.
    faviconLayer.hidden = YES;
    [bgLayer_ addSublayer:faviconLayer];
    [allFaviconLayers_ addObject:faviconLayer];

    CATextLayer* titleLayer = [CATextLayer layer];
    titleLayer.frame = NSRectToCGRect(tile.title_rect());
    titleLayer.string = base::SysUTF16ToNSString(tile.title());
    titleLayer.fontSize = [font pointSize];
    titleLayer.truncationMode = kCATruncationEnd;
    titleLayer.font = font;
    titleLayer.zPosition = 1;  // On top of the thumb shadow.
    titleLayer.hidden = YES;
    [bgLayer_ addSublayer:titleLayer];
    [allTitleLayers_ addObject:titleLayer];
  }
  [self selectTileAtIndex:tileSet_->selected_index()];
}

- (BOOL)canBecomeKeyWindow {
 return YES;
}

- (void)keyDown:(NSEvent*)event {
  // Overridden to prevent beeps.
}

- (void)keyUp:(NSEvent*)event {
  if (state_ == tabpose::kFadingOut)
    return;

  NSString* characters = [event characters];
  if ([characters length] < 1)
    return;

  unichar character = [characters characterAtIndex:0];
  switch (character) {
    case NSEnterCharacter:
    case NSNewlineCharacter:
    case NSCarriageReturnCharacter:
    case ' ':
      [self fadeAway:([event modifierFlags] & NSShiftKeyMask) != 0];
      break;
    case '\e':  // Escape
      tileSet_->ResetSelectedIndex();
      [self fadeAway:([event modifierFlags] & NSShiftKeyMask) != 0];
      break;
    // TODO(thakis): Support moving the selection via arrow keys.
  }
}

- (void)mouseMoved:(NSEvent*)event {
  int newIndex = -1;
  CGPoint p = NSPointToCGPoint([event locationInWindow]);
  for (NSUInteger i = 0; i < [allThumbnailLayers_ count]; ++i) {
    CALayer* layer = [allThumbnailLayers_ objectAtIndex:i];
    CGPoint lp = [layer convertPoint:p fromLayer:rootLayer_];
    if ([static_cast<CALayer*>([layer presentationLayer]) containsPoint:lp])
      newIndex = i;
  }
  if (newIndex >= 0)
    [self selectTileAtIndex:newIndex];
}

- (void)mouseDown:(NSEvent*)event {
  [self fadeAway:([event modifierFlags] & NSShiftKeyMask) != 0];
}

- (void)swipeWithEvent:(NSEvent*)event {
  if ([event deltaY] > 0.5)  // Swipe up
    [self fadeAway:([event modifierFlags] & NSShiftKeyMask) != 0];
}

- (void)close {
  // Prevent parent window from disappearing.
  [[self parentWindow] removeChildWindow:self];
  [super close];
}

- (void)commandDispatch:(id)sender {
  // Without this, -validateUserInterfaceItem: is not called.
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  // Disable all browser-related menu items.
  return NO;
}

- (void)fadeAway:(BOOL)slomo {
  if (state_ == tabpose::kFadingOut)
    return;

  state_ = tabpose::kFadingOut;
  [self setAcceptsMouseMovedEvents:NO];

  // Select chosen tab.
  tabStripModel_->SelectTabContentsAt(tileSet_->selected_index(),
                                      /*user_gesture=*/true);

  {
    ScopedCAActionDisabler disableCAActions;

    // Move the selected layer on top of all other layers.
    [self selectedLayer].zPosition = 1;

    selectionHighlight_.hidden = YES;
    for (CALayer* layer in allFaviconLayers_.get())
      layer.hidden = YES;
    for (CALayer* layer in allTitleLayers_.get())
      layer.hidden = YES;

    // Running animations with shadows is slow, so turn shadows off before
    // running the exit animation.
    for (CALayer* layer in allThumbnailLayers_.get())
      layer.shadowOpacity = 0.0;
  }

  // Animate layers out, all in one transaction.
  CGFloat duration = kDefaultAnimationDuration * (slomo ? kSlomoFactor : 1);
  ScopedCAActionSetDuration durationSetter(duration);
  for (NSUInteger i = 0; i < [allThumbnailLayers_ count]; ++i) {
    CALayer* layer = [allThumbnailLayers_ objectAtIndex:i];
    // |start_thumb_rect_| was relative to |initial_index_|, now this needs to
    // be relative to |selectedIndex_| (whose start rect was relative to
    // |initial_index_| too)
    CGRect newFrame = NSRectToCGRect(
        tileSet_->tile_at(i).GetStartRectRelativeTo(tileSet_->selected_tile()));

    // Add a delegate to one of the implicit animations to get a notification
    // once the animations are done.
    if (static_cast<int>(i) == tileSet_->selected_index()) {
      CAAnimation* animation = [CAAnimation animation];
      animation.delegate = self;
      [animation setValue:kAnimationIdFadeOut forKey:kAnimationIdKey];
      [layer addAnimation:animation forKey:@"frame"];
    }

    layer.frame = newFrame;
  }
}

- (void)animationDidStop:(CAAnimation*)animation finished:(BOOL)finished {
  NSString* animationId = [animation valueForKey:kAnimationIdKey];
  if ([animationId isEqualToString:kAnimationIdFadeIn]) {
    if (finished) {
      // If the user clicks while the fade in animation is still running,
      // |state_| is already kFadingOut. In that case, don't do anything.
      DCHECK_EQ(tabpose::kFadingIn, state_);
      state_ = tabpose::kFadedIn;

      selectionHighlight_.hidden = NO;
      for (CALayer* layer in allFaviconLayers_.get())
        layer.hidden = NO;
      for (CALayer* layer in allTitleLayers_.get())
        layer.hidden = NO;

      // Running animations with shadows is slow, so turn shadows on only after
      // the animation is done.
      ScopedCAActionDisabler disableCAActions;
      for (CALayer* layer in allThumbnailLayers_.get())
        layer.shadowOpacity = 0.5;
    }
  } else if ([animationId isEqualToString:kAnimationIdFadeOut]) {
    DCHECK_EQ(tabpose::kFadingOut, state_);
    [self close];
  }
}

@end

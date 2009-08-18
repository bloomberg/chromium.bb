// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/cocoa/nsimage_cache.h"
#import "chrome/common/cocoa_utils.h"

namespace {

// The size delta between the font used for the edit and the result
// rows.
const int kEditFontAdjust = -1;

// How much to adjust the cell sizing up from the default determined
// by the font.
const int kCellHeightAdjust = 7.0;

// How to round off the popup's corners.  Goal is to match star and go
// buttons.
const CGFloat kPopupRoundingRadius = 4.0;

// How far to offset the image column from the left.
const CGFloat kImageXOffset = 8.0;

// How far to offset the text column from the left.
const CGFloat kTextXOffset = 33.0;

// Background colors for different states of the popup elements.
NSColor* BackgroundColor() {
  return [NSColor controlBackgroundColor];
}
NSColor* SelectedBackgroundColor() {
  return [NSColor selectedControlColor];
}
NSColor* HoveredBackgroundColor() {
  return [NSColor controlColor];
}

// TODO(shess): These are totally unprincipled.  I experimented with
// +controlTextColor and the like, but found myself wondering whether
// that was really appropriate.  Circle back after consulting with
// someone more knowledgeable about the ins and outs of this.
static const NSColor* ContentTextColor() {
  return [NSColor blackColor];
}
static const NSColor* URLTextColor() {
  return [NSColor colorWithCalibratedRed:0.0 green:0.55 blue:0.0 alpha:1.0];
}
static const NSColor* DescriptionTextColor() {
  return [NSColor darkGrayColor];
}

// Return the appropriate icon for the given match.  Derived from the
// gtk code.
NSImage* MatchIcon(const AutocompleteMatch& match) {
  if (match.starred) {
    return nsimage_cache::ImageNamed(@"o2_star.png");
  }

  switch (match.type) {
    case AutocompleteMatch::URL_WHAT_YOU_TYPED:
    case AutocompleteMatch::NAVSUGGEST: {
      return nsimage_cache::ImageNamed(@"o2_globe.png");
    }
    case AutocompleteMatch::HISTORY_URL:
    case AutocompleteMatch::HISTORY_TITLE:
    case AutocompleteMatch::HISTORY_BODY:
    case AutocompleteMatch::HISTORY_KEYWORD: {
      return nsimage_cache::ImageNamed(@"o2_history.png");
    }
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
    case AutocompleteMatch::SEARCH_OTHER_ENGINE: {
      return nsimage_cache::ImageNamed(@"o2_search.png");
    }
    case AutocompleteMatch::OPEN_HISTORY_PAGE: {
      return nsimage_cache::ImageNamed(@"o2_more.png");
    }
    default:
      NOTREACHED();
      break;
  }

  return nil;
}

}  // namespace

// Helper for MatchText() to allow sharing code between the contents
// and description cases.  Returns NSMutableAttributedString as a
// convenience for MatchText().
NSMutableAttributedString* AutocompletePopupViewMac::DecorateMatchedString(
    const std::wstring &matchString,
    const AutocompleteMatch::ACMatchClassifications &classifications,
    NSColor* textColor, NSFont* font) {
  // Cache for on-demand computation of the bold version of |font|.
  NSFont* boldFont = nil;

  // Start out with a string using the default style info.
  NSString* s = base::SysWideToNSString(matchString);
  NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:
                                  font, NSFontAttributeName,
                                  textColor, NSForegroundColorAttributeName,
                                  nil];
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:s
                                              attributes:attributes]
        autorelease];

  // Mark up the runs which differ from the default.
  for (ACMatchClassifications::const_iterator i = classifications.begin();
       i != classifications.end(); ++i) {
    const BOOL isLast = (i+1) == classifications.end();
    const size_t nextOffset = (isLast ? matchString.length() : (i+1)->offset);
    const NSInteger location = static_cast<NSInteger>(i->offset);
    const NSInteger length = static_cast<NSInteger>(nextOffset - i->offset);
    const NSRange range = NSMakeRange(location, length);

    if (0 != (i->style & ACMatchClassification::URL)) {
      [as addAttribute:NSForegroundColorAttributeName
                 value:URLTextColor() range:range];
    }

    if (0 != (i->style & ACMatchClassification::MATCH)) {
      if (!boldFont) {
        NSFontManager* fontManager = [NSFontManager sharedFontManager];
        boldFont = [fontManager convertFont:font toHaveTrait:NSBoldFontMask];
      }
      [as addAttribute:NSFontAttributeName value:boldFont range:range];
    }
  }

  return as;
}

// Return the text to show for the match, based on the match's
// contents and description.  Result will be in |font|, with the
// boldfaced version used for matches.
NSAttributedString* AutocompletePopupViewMac::MatchText(
    const AutocompleteMatch& match, NSFont* font) {
  NSMutableAttributedString *as =
      DecorateMatchedString(match.contents, match.contents_class,
                            ContentTextColor(), font);

  // If there is a description, append it, separated from the contents
  // with an en dash, and decorated with a distinct color.
  if (!match.description.empty()) {
    NSDictionary* attributes =
        [NSDictionary dictionaryWithObjectsAndKeys:
             font, NSFontAttributeName,
             ContentTextColor(), NSForegroundColorAttributeName,
             nil];
    NSString* rawEnDash = [NSString stringWithFormat:@" %C ", 0x2013];
    NSAttributedString* enDash =
        [[[NSAttributedString alloc] initWithString:rawEnDash
                                         attributes:attributes] autorelease];

    NSAttributedString* description =
        DecorateMatchedString(match.description, match.description_class,
                              DescriptionTextColor(), font);

    [as appendAttributedString:enDash];
    [as appendAttributedString:description];
  }

  NSMutableParagraphStyle* style =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  [as addAttribute:NSParagraphStyleAttributeName value:style
             range:NSMakeRange(0, [as length])];

  return as;
}

// AutocompleteButtonCell overrides how backgrounds are displayed to
// handle hover versus selected.  So long as we're in there, it also
// provides some default initialization.

@interface AutocompleteButtonCell : NSButtonCell {
}
@end

// AutocompleteMatrix sets up a tracking area to implement hover by
// highlighting the cell the mouse is over.

@interface AutocompleteMatrix : NSMatrix {
}
@end

// Thin Obj-C bridge class between the target of the popup window's
// AutocompleteMatrix and the AutocompletePopupView implementation.

// TODO(shess): Now that I'm using AutocompleteMatrix, I could instead
// subvert the target/action stuff and have it message popup_view_
// directly.

@interface AutocompleteMatrixTarget : NSObject {
 @private
  AutocompletePopupViewMac* popup_view_;  // weak, owns us.
}
- initWithPopupView:(AutocompletePopupViewMac*)view;

// Tell popup model via popup_view_ about the selected row.
- (void)select:sender;

// Resize the popup when the field's window resizes.
- (void)windowDidResize:(NSNotification*)notification;

@end

AutocompletePopupViewMac::AutocompletePopupViewMac(
    AutocompleteEditViewMac* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    NSTextField* field)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      field_(field),
      matrix_target_([[AutocompleteMatrixTarget alloc] initWithPopupView:this]),
      popup_(nil) {
  DCHECK(edit_view);
  DCHECK(edit_model);
  DCHECK(profile);
  edit_model->set_popup_model(model_.get());
}

AutocompletePopupViewMac::~AutocompletePopupViewMac() {
  // TODO(shess): Having to be aware of destructor ordering in this
  // way seems brittle.  There must be a better way.

  // Destroy the popup model before this object is destroyed, because
  // it can call back to us in the destructor.
  model_.reset();

  // Break references to matrix_target_ before it is released.
  NSMatrix* matrix = [popup_ contentView];
  [matrix setTarget:nil];
}

bool AutocompletePopupViewMac::IsOpen() const {
  return [popup_ isVisible] ? true : false;
}

void AutocompletePopupViewMac::CreatePopupIfNeeded() {
  if (!popup_) {
    popup_.reset([[NSWindow alloc] initWithContentRect:NSZeroRect
                                             styleMask:NSBorderlessWindowMask
                                               backing:NSBackingStoreBuffered
                                                 defer:YES]);
    [popup_ setMovableByWindowBackground:NO];
    // The window will have rounded borders.
    [popup_ setAlphaValue:1.0];
    [popup_ setOpaque:NO];
    [popup_ setLevel:NSNormalWindowLevel];

    AutocompleteMatrix* matrix =
        [[[AutocompleteMatrix alloc] initWithFrame:NSZeroRect] autorelease];
    [matrix setTarget:matrix_target_];
    [matrix setAction:@selector(select:)];
    [popup_ setContentView:matrix];

    // We need the popup to follow window resize.
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:matrix_target_ 
           selector:@selector(windowDidResize:) 
               name:NSWindowDidResizeNotification 
             object:[field_ window]];
  }
}

void AutocompletePopupViewMac::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    [[popup_ parentWindow] removeChildWindow:popup_];
    [popup_ orderOut:nil];

    // Break references to matrix_target_ before releasing popup_.
    NSMatrix* matrix = [popup_ contentView];
    [matrix setTarget:nil];

    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc removeObserver:matrix_target_
                  name:NSWindowDidResizeNotification 
                object:[field_ window]];

    popup_.reset(nil);

    return;
  }

  CreatePopupIfNeeded();

  // The popup's font is a slightly smaller version of what |field_|
  // uses.
  NSFont* fieldFont = [field_ font];
  const CGFloat resultFontSize = [fieldFont pointSize] + kEditFontAdjust;
  NSFont* resultFont =
      [NSFont fontWithName:[fieldFont fontName] size:resultFontSize];

  // Load the results into the popup's matrix.
  AutocompleteMatrix* matrix = [popup_ contentView];
  const size_t rows = model_->result().size();
  DCHECK_GT(rows, 0U);
  [matrix renewRows:rows columns:1];
  for (size_t ii = 0; ii < rows; ++ii) {
    AutocompleteButtonCell* cell = [matrix cellAtRow:ii column:0];
    const AutocompleteMatch& match = model_->result().match_at(ii);
    [cell setImage:MatchIcon(match)];
    [cell setAttributedTitle:MatchText(match, resultFont)];
  }

  // Layout the popup and size it to land underneath the field.
  // TODO(shess) Consider refactoring to remove this depenency,
  // because the popup doesn't need any of the field-like aspects of
  // field_.  The edit view could expose helper methods for attaching
  // the window to the field.

  // Locate |field_| on the screen, and pad the left and right sides
  // by the height to make it wide enough to include the star and go
  // buttons.
  // TODO(shess): This assumes that those buttons will be square.
  // Better to plumb through so that this code can get the rect from
  // the toolbar controller?
  NSRect r = [field_ convertRect:[field_ bounds] toView:nil];
  r.origin.x -= r.size.height;
  r.size.width += 2 * r.size.height;
  r.origin = [[field_ window] convertBaseToScreen:r.origin];
  DCHECK_GT(r.size.width, 0.0);

  // Set the cell size to fit a line of text in the cell's font.  All
  // cells should use the same font and each should layout in one
  // line, so they should all be about the same height.
  const NSSize cellSize = [[matrix cellAtRow:0 column:0] cellSize];
  DCHECK_GT(cellSize.height, 0.0);
  [matrix setCellSize:NSMakeSize(r.size.width,
                                 cellSize.height + kCellHeightAdjust)];

  // Make the matrix big enough to hold all the cells.
  [matrix sizeToCells];

  // Make the window just as big.
  r.size.height = [matrix frame].size.height;
  r.origin.y -= r.size.height + 2;

  // Update the selection.
  PaintUpdatesNow();

  [popup_ setFrame:r display:YES];

  if (!IsOpen()) {
    [[field_ window] addChildWindow:popup_ ordered:NSWindowAbove];
  }
}

// This is only called by model in SetSelectedLine() after updating
// everything.  Popup should already be visible.
void AutocompletePopupViewMac::PaintUpdatesNow() {
  AutocompleteMatrix* matrix = [popup_ contentView];
  [matrix selectCellAtRow:model_->selected_line() column:0];
}

AutocompletePopupModel* AutocompletePopupViewMac::GetModel() {
  return model_.get();
}

void AutocompletePopupViewMac::AcceptInput() {
  const NSInteger selectedRow = [[popup_ contentView] selectedRow];

  // -1 means no cells were selected.  This can happen if the user
  // clicked and then dragged their mouse off the popup before
  // releasing, so reset the selection and ignore the event.
  if (selectedRow == -1) {
    PaintUpdatesNow();
  } else {
    model_->SetSelectedLine(selectedRow, false);
    WindowOpenDisposition disposition = event_utils::DispositionFromEventFlags(
          [[NSApp currentEvent] modifierFlags]);
    edit_view_->AcceptInput(disposition, false);
  }
}

@implementation AutocompleteButtonCell

- init {
  self = [super init];
  if (self) {
    [self setImagePosition:NSImageLeft];
    [self setBordered:NO];
    [self setButtonType:NSRadioButton];

    // Without this highlighting messes up white areas of images.
    [self setHighlightsBy:NSNoCellMask];
  }
  return self;
}

- (NSColor*)backgroundColor {
  if ([self state] == NSOnState) {
    return SelectedBackgroundColor();
  } else if ([self isHighlighted]) {
    return HoveredBackgroundColor();
  }
  return BackgroundColor();
}

// The default NSButtonCell drawing leaves the image flush left and
// the title next to the image.  This spaces things out to line up
// with the star button and autocomplete field.
// TODO(shess): Determine if the star button can change size (other
// than scaling coordinates), in which case this needs to be more
// dynamic.
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  [[self backgroundColor] set];
  NSRectFill(cellFrame);

  // Put the image centered vertically but in a fixed column.
  // TODO(shess) Currently, the images are all 16x16 png files, so
  // left-justified works out fine.  If that changes, it may be
  // appropriate to align them on their centers instead of their
  // left-hand sides.
  NSImage* image = [self image];
  if (image) {
    NSRect imageRect = cellFrame;
    imageRect.size = [image size];
    imageRect.origin.y +=
        floor((NSHeight(cellFrame) - NSHeight(imageRect)) / 2);
    imageRect.origin.x += kImageXOffset;
    [self drawImage:image withFrame:imageRect inView:controlView];
  }

  // Adjust the title position to be lined up under the field's text.
  NSAttributedString* title = [self attributedTitle];
  if (title) {
    NSRect titleRect = cellFrame;
    titleRect.size.width -= kTextXOffset;
    titleRect.origin.x += kTextXOffset;
    [self drawTitle:title withFrame:titleRect inView:controlView];
  }
}

@end

@implementation AutocompleteMatrix

// Remove all tracking areas and initialize the one we want.  Removing
// all might be overkill, but it's unclear why there would be others
// for the popup window.
- (void)resetTrackingArea {
  for (NSTrackingArea* trackingArea in [self trackingAreas]) {
    [self removeTrackingArea:trackingArea];
  }

  // TODO(shess): Consider overriding -acceptsFirstMouse: and changing
  // NSTrackingActiveInActiveApp to NSTrackingActiveAlways.
  NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited;
  options |= NSTrackingMouseMoved;
  options |= NSTrackingActiveInActiveApp;
  options |= NSTrackingInVisibleRect;

  NSTrackingArea* trackingArea =
      [[[NSTrackingArea alloc] initWithRect:[self frame]
                                    options:options
                                      owner:self
                                   userInfo:nil] autorelease];
  [self addTrackingArea:trackingArea];
}

- (void)updateTrackingAreas {
  [self resetTrackingArea];
  [super updateTrackingAreas];
}

- initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setCellClass:[AutocompleteButtonCell class]];

    // Cells pack with no spacing.
    [self setIntercellSpacing:NSMakeSize(0.0, 0.0)];

    [self setDrawsBackground:YES];
    [self setBackgroundColor:BackgroundColor()];
    [self renewRows:0 columns:1];
    [self setAllowsEmptySelection:YES];
    [self setMode:NSRadioModeMatrix];
    [self deselectAllCells];

    [self resetTrackingArea];
  }
  return self;
}

- (void)highlightRowAt:(NSInteger)rowIndex {
  // highlightCell will be nil if rowIndex is out of range, so no cell
  // will be highlighted.
  NSCell* highlightCell = [self cellAtRow:rowIndex column:0];

  for (NSCell* cell in [self cells]) {
    [cell setHighlighted:(cell == highlightCell)];
  }
}

- (void)highlightRowUnder:(NSEvent*)theEvent {
  NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  NSInteger row, column;
  if ([self getRow:&row column:&column forPoint:point]) {
    [self highlightRowAt:row];
  } else {
    [self highlightRowAt:-1];
  }
}

// Callbacks from NSTrackingArea.
- (void)mouseEntered:(NSEvent*)theEvent {
  [self highlightRowUnder:theEvent];
}
- (void)mouseMoved:(NSEvent*)theEvent {
  [self highlightRowUnder:theEvent];
}
- (void)mouseExited:(NSEvent*)theEvent {
  [self highlightRowAt:-1];
}

// This handles drawing the decorations of the rounded popup window,
// calling on NSMatrix to draw the actual contents.
- (void)drawRect:(NSRect)rect {
  // Background clear so we can round the corners.
  [[NSColor clearColor] set];
  NSRectFill([self frame]);

  // The toolbar items we're mirroring for shape are inset slightly
  // for width.  I don't know why, which is why I didn't make this a
  // constant, yet.  The factor of 0.5 on both dimensions is to put
  // the stroke down the middle of the pixels.
  const NSRect border(NSInsetRect([self bounds], 1.5, 0.5));
  NSBezierPath* path =
      [NSBezierPath bezierPathWithRoundedRect:border
                                      xRadius:kPopupRoundingRadius
                                      yRadius:kPopupRoundingRadius];

  // Draw the matrix clipped to our border.
  [NSGraphicsContext saveGraphicsState];
  [path addClip];
  [super drawRect:rect];
  [NSGraphicsContext restoreGraphicsState];

  // Put a border over that.
  // TODO(shess): Theme the color?
  [[NSColor lightGrayColor] setStroke];
  [path setLineWidth:1.0];
  [path stroke];
}

@end

@implementation AutocompleteMatrixTarget

- initWithPopupView:(AutocompletePopupViewMac*)view {
  self = [super init];
  if (self) {
    popup_view_ = view;
  }
  return self;
}

- (void)select:sender {
  DCHECK(popup_view_);
  popup_view_->AcceptInput();
}

- (void)windowDidResize:(NSNotification*)notification {
  DCHECK(popup_view_);

  // TODO(shess): UpdatePopupAppearance() is called frequently, so it
  // should be really cheap, but in this case we could probably make
  // things even cheaper by refactoring between the popup-placement
  // code and the matrix-population code.
  popup_view_->UpdatePopupAppearance();
}

@end

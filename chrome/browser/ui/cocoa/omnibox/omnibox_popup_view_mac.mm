// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/omnibox/omnibox_popup_view_mac.h"

#include <cmath>

#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_separator_view.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/common/autocomplete_match_type.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#import "ui/base/cocoa/cocoa_event_utils.h"
#import "ui/base/cocoa/flipped_view.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/rect.h"

namespace {

// How much to adjust the cell sizing up from the default determined
// by the font.
const CGFloat kCellHeightAdjust = 6.0;

// Padding between matrix and the top and bottom of the popup window.
const CGFloat kPopupPaddingVertical = 5.0;

// Animation duration when animating the popup window smaller.
const NSTimeInterval kShrinkAnimationDuration = 0.1;

// Background colors for different states of the popup elements.
NSColor* BackgroundColor() {
  return [NSColor controlBackgroundColor];
}

NSColor* ContentTextColor() {
  return [NSColor blackColor];
}
NSColor* DimContentTextColor() {
  return [NSColor darkGrayColor];
}
NSColor* URLTextColor() {
  return [NSColor colorWithCalibratedRed:0.0 green:0.55 blue:0.0 alpha:1.0];
}

}  // namespace

OmniboxPopupViewMac::OmniboxPopupViewMac(OmniboxView* omnibox_view,
                                         OmniboxEditModel* edit_model,
                                         NSTextField* field)
    : omnibox_view_(omnibox_view),
      model_(new OmniboxPopupModel(this, edit_model)),
      field_(field),
      popup_(nil),
      target_popup_frame_(NSZeroRect) {
  DCHECK(omnibox_view);
  DCHECK(edit_model);
}

OmniboxPopupViewMac::~OmniboxPopupViewMac() {
  // Destroy the popup model before this object is destroyed, because
  // it can call back to us in the destructor.
  model_.reset();

  // Break references to |this| because the popup may not be
  // deallocated immediately.
  [matrix_ setDelegate:NULL];
}

bool OmniboxPopupViewMac::IsOpen() const {
  return popup_ != nil;
}

void OmniboxPopupViewMac::UpdatePopupAppearance() {
  DCHECK([NSThread isMainThread]);
  const AutocompleteResult& result = GetResult();
  const size_t start_match = result.ShouldHideTopMatch() ? 1 : 0;
  const size_t rows = result.size() - start_match;
  if (rows == 0) {
    [[popup_ parentWindow] removeChildWindow:popup_];
    [popup_ orderOut:nil];

    // Break references to |this| because the popup may not be
    // deallocated immediately.
    [matrix_ setDelegate:nil];
    matrix_.reset();

    popup_.reset(nil);

    target_popup_frame_ = NSZeroRect;

    return;
  }

  CreatePopupIfNeeded();

  gfx::Font result_font(OmniboxViewMac::GetFieldFont());

  // Calculate the width of the matrix based on backing out the popup's border
  // from the width of the field.
  const CGFloat matrix_width = NSWidth([field_ bounds]);
  DCHECK_GT(matrix_width, 0.0);

  // Load the results into the popup's matrix.
  DCHECK_GT(rows, 0U);
  [matrix_ renewRows:rows columns:1];
  for (size_t ii = 0; ii < rows; ++ii) {
    OmniboxPopupCell* cell = [matrix_ cellAtRow:ii column:0];
    const AutocompleteMatch& match = GetResult().match_at(ii + start_match);
    [cell setImage:ImageForMatch(match)];
    [cell setContentText:DecorateMatchedString(match.contents,
                                               match.contents_class,
                                               ContentTextColor(),
                                               DimContentTextColor(),
                                               result_font)];
    if (match.description.empty()) {
      [cell setDescriptionText:nil];
    } else {
      [cell setDescriptionText:DecorateMatchedString(match.description,
                                                     match.description_class,
                                                     DimContentTextColor(),
                                                     DimContentTextColor(),
                                                     result_font)];
    }
  }

  // Set the cell size to fit a line of text in the cell's font.  All
  // cells should use the same font and each should layout in one
  // line, so they should all be about the same height.
  const NSSize cell_size = [[matrix_ cellAtRow:0 column:0] cellSize];
  DCHECK_GT(cell_size.height, 0.0);
  const CGFloat cell_height = cell_size.height + kCellHeightAdjust;
  [matrix_ setCellSize:NSMakeSize(matrix_width, cell_height)];

  // Update the selection before placing (and displaying) the window.
  PaintUpdatesNow();

  // Calculate the matrix size manually rather than using -sizeToCells
  // because actually resizing the matrix messed up the popup size
  // animation.
  DCHECK_EQ([matrix_ intercellSpacing].height, 0.0);
  PositionPopup(rows * cell_height);
}

gfx::Rect OmniboxPopupViewMac::GetTargetBounds() {
  // Flip the coordinate system before returning.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect monitor_frame = [screen frame];
  gfx::Rect bounds(NSRectToCGRect(target_popup_frame_));
  bounds.set_y(monitor_frame.size.height - bounds.y() - bounds.height());
  return bounds;
}

// This is only called by model in SetSelectedLine() after updating
// everything.  Popup should already be visible.
void OmniboxPopupViewMac::PaintUpdatesNow() {
  size_t start_match = model_->result().ShouldHideTopMatch() ? 1 : 0;
  if (start_match > model_->selected_line()) {
    [matrix_ deselectAllCells];
  } else {
    [matrix_ selectCellAtRow:model_->selected_line() - start_match column:0];
  }

}

void OmniboxPopupViewMac::OnMatrixRowSelected(OmniboxPopupMatrix* matrix,
                                              size_t row) {
  size_t start_match = model_->result().ShouldHideTopMatch() ? 1 : 0;
  model_->SetSelectedLine(row + start_match, false, false);
}

void OmniboxPopupViewMac::OnMatrixRowClicked(OmniboxPopupMatrix* matrix,
                                             size_t row) {
  OpenURLForRow(row,
                ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]));
}

void OmniboxPopupViewMac::OnMatrixRowMiddleClicked(OmniboxPopupMatrix* matrix,
                                                   size_t row) {
  OpenURLForRow(row, NEW_BACKGROUND_TAB);
}

// static
NSMutableAttributedString* OmniboxPopupViewMac::DecorateMatchedString(
    const string16& match_string,
    const AutocompleteMatch::ACMatchClassifications& classifications,
    NSColor* text_color,
    NSColor* dim_text_color,
    gfx::Font& font) {
  // Cache for on-demand computation of the bold version of |font|.
  NSFont* bold_font = nil;

  // Start out with a string using the default style info.
  NSString* s = base::SysUTF16ToNSString(match_string);
  NSDictionary* attributes = @{
      NSFontAttributeName : font.GetNativeFont(),
      NSForegroundColorAttributeName : text_color
  };
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:s
                                              attributes:attributes]
        autorelease];

  // As a protective measure, bail if the length of the match string is not
  // the same as the length of the converted NSString. http://crbug.com/121703
  if ([s length] != match_string.size())
    return as;

  // Mark up the runs which differ from the default.
  for (ACMatchClassifications::const_iterator i = classifications.begin();
       i != classifications.end(); ++i) {
    const BOOL is_last = (i+1) == classifications.end();
    const NSInteger next_offset =
        (is_last ? [s length] : static_cast<NSInteger>((i + 1)->offset));
    const NSInteger location = static_cast<NSInteger>(i->offset);
    const NSInteger length = next_offset - static_cast<NSInteger>(i->offset);
    // Guard against bad, off-the-end classification ranges.
    if (i->offset >= [s length] || length <= 0)
      break;
    const NSRange range = NSMakeRange(location,
        MIN(length, static_cast<NSInteger>([s length]) - location));

    if (0 != (i->style & ACMatchClassification::URL)) {
      [as addAttribute:NSForegroundColorAttributeName
                 value:URLTextColor()
                 range:range];
    }

    if (0 != (i->style & ACMatchClassification::MATCH)) {
      if (!bold_font) {
        NSFontManager* font_manager = [NSFontManager sharedFontManager];
        bold_font = [font_manager convertFont:font.GetNativeFont()
                                  toHaveTrait:NSBoldFontMask];
      }
      [as addAttribute:NSFontAttributeName value:bold_font range:range];
    }

    if (0 != (i->style & ACMatchClassification::DIM)) {
      [as addAttribute:NSForegroundColorAttributeName
                 value:dim_text_color
                 range:range];
    }
  }

  return as;
}

const AutocompleteResult& OmniboxPopupViewMac::GetResult() const {
  return model_->result();
}

void OmniboxPopupViewMac::CreatePopupIfNeeded() {
  if (!popup_) {
    popup_.reset(
        [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                    styleMask:NSBorderlessWindowMask
                                      backing:NSBackingStoreBuffered
                                        defer:YES]);
    [popup_ setBackgroundColor:[NSColor clearColor]];
    [popup_ setOpaque:NO];

    // Use a flipped view to pin the matrix top the top left. This is needed
    // for animated resize.
    base::scoped_nsobject<FlippedView> contentView(
        [[FlippedView alloc] initWithFrame:NSZeroRect]);
    [popup_ setContentView:contentView];

    // View to draw a background beneath the matrix.
    background_view_.reset([[NSBox alloc] initWithFrame:NSZeroRect]);
    [background_view_ setBoxType:NSBoxCustom];
    [background_view_ setBorderType:NSNoBorder];
    [background_view_ setFillColor:BackgroundColor()];
    [background_view_ setContentViewMargins:NSZeroSize];
    [contentView addSubview:background_view_];

    matrix_.reset([[OmniboxPopupMatrix alloc] initWithDelegate:this]);
    [background_view_ addSubview:matrix_];

    top_separator_view_.reset(
        [[OmniboxPopupTopSeparatorView alloc] initWithFrame:NSZeroRect]);
    [contentView addSubview:top_separator_view_];

    bottom_separator_view_.reset(
        [[OmniboxPopupBottomSeparatorView alloc] initWithFrame:NSZeroRect]);
    [contentView addSubview:bottom_separator_view_];

    // TODO(dtseng): Ignore until we provide NSAccessibility support.
    [popup_ accessibilitySetOverrideValue:NSAccessibilityUnknownRole
                             forAttribute:NSAccessibilityRoleAttribute];
  }
}

void OmniboxPopupViewMac::PositionPopup(const CGFloat matrixHeight) {
  BrowserWindowController* controller =
      [BrowserWindowController browserWindowControllerForView:field_];
  NSRect anchor_rect_base = [controller omniboxPopupAnchorRect];

  // Calculate the popup's position on the screen.
  NSRect popup_frame = anchor_rect_base;
  // Size to fit the matrix and shift down by the size.
  popup_frame.size.height = matrixHeight + kPopupPaddingVertical * 2.0;
  popup_frame.size.height += [OmniboxPopupTopSeparatorView preferredHeight];
  popup_frame.size.height += [OmniboxPopupBottomSeparatorView preferredHeight];
  popup_frame.origin.y -= NSHeight(popup_frame);
  // Shift to screen coordinates.
  popup_frame.origin =
      [[controller window] convertBaseToScreen:popup_frame.origin];

  // Do nothing if the popup is already animating to the given |frame|.
  if (NSEqualRects(popup_frame, target_popup_frame_))
    return;

  // Top separator.
  NSRect top_separator_frame = NSZeroRect;
  top_separator_frame.size.width = NSWidth(popup_frame);
  top_separator_frame.size.height =
      [OmniboxPopupTopSeparatorView preferredHeight];
  [top_separator_view_ setFrame:top_separator_frame];

  // Bottom separator.
  NSRect bottom_separator_frame = NSZeroRect;
  bottom_separator_frame.size.width = NSWidth(popup_frame);
  bottom_separator_frame.size.height =
      [OmniboxPopupBottomSeparatorView preferredHeight];
  bottom_separator_frame.origin.y =
      NSHeight(popup_frame) - NSHeight(bottom_separator_frame);
  [bottom_separator_view_ setFrame:bottom_separator_frame];

  // Background view.
  NSRect background_rect = NSZeroRect;
  background_rect.size.width = NSWidth(popup_frame);
  background_rect.size.height = NSHeight(popup_frame) -
      NSHeight(top_separator_frame) - NSHeight(bottom_separator_frame);
  background_rect.origin.y = NSMaxY(top_separator_frame);
  [background_view_ setFrame:background_rect];

  // Matrix.
  NSPoint field_origin_base =
      [field_ convertPoint:[field_ bounds].origin toView:nil];
  NSRect matrix_frame = NSZeroRect;
  matrix_frame.origin.x = field_origin_base.x - NSMinX(anchor_rect_base);
  matrix_frame.origin.y = kPopupPaddingVertical;
  matrix_frame.size.width = [matrix_ cellSize].width;
  matrix_frame.size.height = matrixHeight;
  [matrix_ setFrame:matrix_frame];

  NSRect current_poup_frame = [popup_ frame];
  target_popup_frame_ = popup_frame;

  // Animate the frame change if the only change is that the height got smaller.
  // Otherwise, resize immediately.
  bool animate = (NSHeight(popup_frame) < NSHeight(current_poup_frame) &&
                  NSWidth(popup_frame) == NSWidth(current_poup_frame));

  base::scoped_nsobject<NSDictionary> savedAnimations;
  if (!animate) {
    // In an ideal world, running a zero-length animation would cancel any
    // running animations and set the new frame value immediately.  In practice,
    // zero-length animations are ignored entirely.  Work around this AppKit bug
    // by explicitly setting an NSNull animation for the "frame" key and then
    // running the animation with a non-zero(!!) duration.  This somehow
    // convinces AppKit to do the right thing.  Save off the current animations
    // dictionary so it can be restored later.
    savedAnimations.reset([[popup_ animations] copy]);
    [popup_ setAnimations:@{@"frame" : [NSNull null]}];
  }

  [NSAnimationContext beginGrouping];
  // Don't use the GTM additon for the "Steve" slowdown because this can happen
  // async from user actions and the effects could be a surprise.
  [[NSAnimationContext currentContext] setDuration:kShrinkAnimationDuration];
  [[popup_ animator] setFrame:popup_frame display:YES];
  [NSAnimationContext endGrouping];

  if (!animate) {
    // Restore the original animations dictionary.  This does not reinstate any
    // previously running animations.
    [popup_ setAnimations:savedAnimations];
  }

  if (![popup_ isVisible])
    [[field_ window] addChildWindow:popup_ ordered:NSWindowAbove];
}

NSImage* OmniboxPopupViewMac::ImageForMatch(const AutocompleteMatch& match) {
  gfx::Image image = model_->GetIconIfExtensionMatch(match);
  if (!image.IsEmpty())
    return image.AsNSImage();

  const int resource_id = match.starred ?
      IDR_OMNIBOX_STAR : AutocompleteMatch::TypeToIcon(match.type);
  return OmniboxViewMac::ImageForResource(resource_id);
}

void OmniboxPopupViewMac::OpenURLForRow(size_t row,
                                        WindowOpenDisposition disposition) {
  size_t start_match = model_->result().ShouldHideTopMatch() ? 1 : 0;
  row += start_match;
  DCHECK_LT(row, GetResult().size());

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = GetResult().match_at(row);
  omnibox_view_->OpenMatch(match, disposition, GURL(), row);
}

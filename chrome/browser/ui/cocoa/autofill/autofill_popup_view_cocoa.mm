// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_popup_view_cocoa.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_popup_view_bridge_views.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/web_textfield_touch_bar_controller.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/grit/components_scaled_resources.h"
#include "components/toolbar/vector_icons.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkColor.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_mac.h"

using autofill::AutofillPopupView;
using autofill::AutofillPopupLayoutModel;
using base::SysUTF16ToNSString;

namespace {

// Touch bar identifier.
NSString* const kCreditCardAutofillTouchBarId = @"credit-card-autofill";

// Touch bar items identifiers.
NSString* const kCreditCardTouchId = @"CREDIT-CARD";
NSString* const kCreditCardItemsTouchId = @"CREDIT-CARD-ITEMS";

// Maximum number of autofill items that can appear on the touch bar.
constexpr int maxTouchBarItems = 3;

// Returns the credit card image.
NSImage* GetCreditCardTouchBarImage(int iconId) {
  if (iconId == -1)
    return nil;

  // If it's a generic card image, use the vector icon instead.
  if (iconId == IDR_AUTOFILL_CC_GENERIC) {
    return NSImageFromImageSkia(
        gfx::CreateVectorIcon(kCreditCardIcon, 16, SK_ColorWHITE));
  }

  return ui::ResourceBundle::GetSharedInstance()
      .GetNativeImageNamed(iconId)
      .AsNSImage();
}

}  // namespace

@interface AutofillPopupViewCocoa ()

#pragma mark -
#pragma mark Private methods

// Draws an Autofill suggestion in the given |bounds|, labeled with the given
// |name| and |subtext| hint.  If the suggestion |isSelected|, then it is drawn
// with a highlight.  |index| determines the font to use, as well as the icon,
// if the row requires it -- such as for credit cards.
- (void)drawSuggestionWithName:(NSString*)name
                       subtext:(NSString*)subtext
                         index:(NSInteger)index
                        bounds:(NSRect)bounds
                      selected:(BOOL)isSelected
                   textYOffset:(CGFloat)textYOffset;

// This comment block applies to all three draw* methods that follow.
// If |rightAlign| == YES.
//   Draws the widget with right border aligned to |x|.
//   Returns the x value of left border of the widget.
// If |rightAlign| == NO.
//   Draws the widget with left border aligned to |x|.
//   Returns the x value of right border of the widget.
- (CGFloat)drawName:(NSString*)name
                atX:(CGFloat)x
              index:(NSInteger)index
         rightAlign:(BOOL)rightAlign
             bounds:(NSRect)bounds
        textYOffset:(CGFloat)textYOffset;
- (CGFloat)drawIconAtIndex:(NSInteger)index
                       atX:(CGFloat)x
                rightAlign:(BOOL)rightAlign
                    bounds:(NSRect)bounds;
- (CGFloat)drawSubtext:(NSString*)subtext
                   atX:(CGFloat)x
                 index:(NSInteger)index
            rightAlign:(BOOL)rightAlign
                bounds:(NSRect)bounds
           textYOffset:(CGFloat)textYOffset;

// Returns the icon for the row with the given |index|, or |nil| if there is
// none.
- (NSImage*)iconAtIndex:(NSInteger)index;

- (NSColor*)touchBarSubtextColor;

@end

@implementation AutofillPopupViewCocoa

#pragma mark -
#pragma mark Initialisers

- (id)initWithFrame:(NSRect)frame {
  NOTREACHED();
  return [self initWithController:NULL frame:frame delegate:NULL];
}

- (id)initWithController:(autofill::AutofillPopupController*)controller
                   frame:(NSRect)frame
                delegate:(autofill::AutofillPopupViewCocoaDelegate*)delegate {
  self = [super initWithDelegate:controller frame:frame];
  if (self) {
    controller_ = controller;
    delegate_ = delegate;
  }

  return self;
}

#pragma mark -
#pragma mark NSView implementation:

- (void)drawRect:(NSRect)dirtyRect {
  // If the view is in the process of being destroyed, don't bother drawing.
  if (!controller_)
    return;

  [self drawBackgroundAndBorder];

  for (int i = 0; i < controller_->GetLineCount(); ++i) {
    // Skip rows outside of the dirty rect.
    NSRect rowBounds = NSRectFromCGRect(delegate_->GetRowBounds(i).ToCGRect());
    if (!NSIntersectsRect(rowBounds, dirtyRect))
      continue;
    const autofill::Suggestion& suggestion = controller_->GetSuggestionAt(i);

    if (suggestion.frontend_id == autofill::POPUP_ITEM_ID_SEPARATOR) {
      [self drawSeparatorWithBounds:rowBounds];
      continue;
    }

    // Additional offset applied to the text in the vertical direction.
    CGFloat textYOffset = 0;

    NSString* value = SysUTF16ToNSString(controller_->GetElidedValueAt(i));
    NSString* label = SysUTF16ToNSString(controller_->GetElidedLabelAt(i));
    BOOL isSelected =
        controller_->selected_line() && i == *controller_->selected_line();
    [self drawSuggestionWithName:value
                         subtext:label
                           index:i
                          bounds:rowBounds
                        selected:isSelected
                     textYOffset:textYOffset];
  }
}

#pragma mark -
#pragma mark NSTouchBarDelegate implementation:

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
    API_AVAILABLE(macos(10.12.2)) {
  if (![identifier hasSuffix:kCreditCardItemsTouchId])
    return nil;

  NSMutableArray* creditCardItems = [NSMutableArray array];
  for (int i = 0; i < controller_->GetLineCount() && i < maxTouchBarItems;
       i++) {
    const autofill::Suggestion& suggestion = controller_->GetSuggestionAt(i);
    if (suggestion.frontend_id < autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY)
      continue;

    NSString* identifier = [NSString
        stringWithFormat:@"%@-%i",
                         ui::GetTouchBarItemId(kCreditCardAutofillTouchBarId,
                                               kCreditCardTouchId),
                         i];

    base::scoped_nsobject<NSCustomTouchBarItem> item(
        [[ui::NSCustomTouchBarItem() alloc] initWithIdentifier:identifier]);
    NSString* label = SysUTF16ToNSString(controller_->GetElidedValueAt(i));
    NSString* subtext = SysUTF16ToNSString(controller_->GetElidedLabelAt(i));

    // Create the button title based on the text direction.
    NSString* buttonTitle =
        [subtext length] ? [NSString stringWithFormat:@"%@ %@", label, subtext]
                         : label;

    // Create the button.
    NSImage* cardIconImage = GetCreditCardTouchBarImage(
        controller_->layout_model().GetIconResourceID(suggestion.icon));
    NSButton* button = nil;
    if (cardIconImage) {
      button = [NSButton buttonWithTitle:buttonTitle
                                   image:cardIconImage
                                  target:self
                                  action:@selector(acceptCreditCard:)];
      button.imageHugsTitle = YES;
      button.imagePosition = controller_->IsRTL() ? NSImageLeft : NSImageRight;
    } else {
      button = [NSButton buttonWithTitle:buttonTitle
                                  target:self
                                  action:@selector(acceptCreditCard:)];
    }

    // Apply text attributes to the button so that the subtext will appear
    // smaller and lighter than the rest of the title.
    base::scoped_nsobject<NSMutableAttributedString> attributedString(
        [[NSMutableAttributedString alloc]
            initWithAttributedString:button.attributedTitle]);
    NSFont* subtextFont = [[NSFontManager sharedFontManager]
        convertFont:button.font
             toSize:button.font.pointSize - 1];
    NSRange labelRange = NSMakeRange(0, label.length);
    NSRange subtextRange =
        NSMakeRange(buttonTitle.length - subtext.length, subtext.length);
    [attributedString addAttribute:NSForegroundColorAttributeName
                             value:[NSColor whiteColor]
                             range:labelRange];
    [attributedString addAttribute:NSForegroundColorAttributeName
                             value:[self touchBarSubtextColor]
                             range:subtextRange];
    [attributedString addAttribute:NSFontAttributeName
                             value:subtextFont
                             range:subtextRange];
    button.attributedTitle = attributedString;

    // The tag is used to store the suggestion index.
    button.tag = i;

    [item setView:button];
    [creditCardItems addObject:item.autorelease()];
  }

  return [ui::NSGroupTouchBarItem() groupItemWithIdentifier:identifier
                                                      items:creditCardItems];
}

#pragma mark -
#pragma mark Public API:

- (void)controllerDestroyed {
  // Since the |controller_| either already has been destroyed or is about to
  // be, about the only thing we can safely do with it is to null it out.
  controller_ = NULL;
  [super delegateDestroyed];
}

- (void)invalidateRow:(NSInteger)row {
  NSRect dirty_rect = NSRectFromCGRect(delegate_->GetRowBounds(row).ToCGRect());
  [self setNeedsDisplayInRect:dirty_rect];
}

- (NSTouchBar*)makeTouchBar {
  if (!autofill::IsCreditCardAutofillTouchBarExperimentEnabled())
    return nil;

  if (!controller_->GetLineCount() ||
      !controller_->layout_model().is_credit_card_popup()) {
    return nil;
  }

  base::scoped_nsobject<NSTouchBar> touchBar([[ui::NSTouchBar() alloc] init]);
  [touchBar setCustomizationIdentifier:ui::GetTouchBarId(
                                           kCreditCardAutofillTouchBarId)];
  [touchBar setDelegate:self];

  [touchBar setDefaultItemIdentifiers:@[ kCreditCardItemsTouchId ]];
  return touchBar.autorelease();
}

- (void)showPopup {
  [super showPopup];

  // The following code may show/update the touch bar by eventually calling
  // into |makeTouchBar|.
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:[[self window] parentWindow]];
  TabContentsController* tabContentsController =
      [[bwc tabStripController] activeTabContentsController];
  WebTextfieldTouchBarController* touchBarController =
      [tabContentsController webTextfieldTouchBarController];
  [touchBarController showCreditCardAutofillForPopupView:self];
}

#pragma mark -
#pragma mark Private API:

- (void)drawSuggestionWithName:(NSString*)name
                       subtext:(NSString*)subtext
                         index:(NSInteger)index
                        bounds:(NSRect)bounds
                      selected:(BOOL)isSelected
                   textYOffset:(CGFloat)textYOffset {
  const int frontenId = controller_->GetSuggestionAt(index).frontend_id;
  const bool iconInFrontOfText =
      frontenId == autofill::POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE;

  // If this row is selected, highlight it with this mac system color.
  // Otherwise the controller may have a specific background color for this
  // entry.
  if (isSelected) {
    [[self highlightColor] set];
    [NSBezierPath fillRect:bounds];
  } else {
    SkColor backgroundColor =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
        controller_->GetBackgroundColorIDForRow(index));
    [skia::SkColorToSRGBNSColor(backgroundColor) set];
    [NSBezierPath fillRect:bounds];
  }

  BOOL isRTL = controller_->IsRTL();

  // The X values of the left and right borders of the autofill widget.
  CGFloat leftX = NSMinX(bounds) + AutofillPopupLayoutModel::kEndPadding;
  CGFloat rightX = NSMaxX(bounds) - AutofillPopupLayoutModel::kEndPadding;

  // Draw left side if isRTL == NO, right side if isRTL == YES.
  CGFloat x = isRTL ? rightX : leftX;
  if (iconInFrontOfText) {
    x = [self drawIconAtIndex:index atX:x rightAlign:isRTL bounds:bounds];
  }
  [self drawName:name
              atX:x
            index:index
       rightAlign:isRTL
           bounds:bounds
      textYOffset:textYOffset];

  // Draw right side if isRTL == NO, left side if isRTL == YES.
  x = isRTL ? leftX : rightX;
  if (!iconInFrontOfText) {
    x = [self drawIconAtIndex:index atX:x rightAlign:!isRTL bounds:bounds];
  }
  [self drawSubtext:subtext
                atX:x
              index:index
         rightAlign:!isRTL
             bounds:bounds
        textYOffset:textYOffset];
}

- (CGFloat)drawName:(NSString*)name
                atX:(CGFloat)x
              index:(NSInteger)index
         rightAlign:(BOOL)rightAlign
             bounds:(NSRect)bounds
        textYOffset:(CGFloat)textYOffset {
  NSColor* nameColor = skia::SkColorToSRGBNSColor(
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          controller_->layout_model().GetValueFontColorIDForRow(index)));
  NSDictionary* nameAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:controller_->layout_model()
                                       .GetValueFontListForRow(index)
                                       .GetPrimaryFont()
                                       .GetNativeFont(),
                                   NSFontAttributeName, nameColor,
                                   NSForegroundColorAttributeName, nil];
  NSSize nameSize = [name sizeWithAttributes:nameAttributes];
  x -= rightAlign ? nameSize.width : 0;
  CGFloat y = bounds.origin.y + (bounds.size.height - nameSize.height) / 2;
  y += textYOffset;

  [name drawAtPoint:NSMakePoint(x, y) withAttributes:nameAttributes];

  x += rightAlign ? 0 : nameSize.width;
  return x;
}

- (CGFloat)drawIconAtIndex:(NSInteger)index
                       atX:(CGFloat)x
                rightAlign:(BOOL)rightAlign
                    bounds:(NSRect)bounds {
  NSImage* icon = [self iconAtIndex:index];
  if (!icon)
    return x;
  NSSize iconSize = [icon size];
  x -= rightAlign ? iconSize.width : 0;
  CGFloat y = bounds.origin.y + (bounds.size.height - iconSize.height) / 2;
    [icon drawInRect:NSMakeRect(x, y, iconSize.width, iconSize.height)
            fromRect:NSZeroRect
           operation:NSCompositeSourceOver
            fraction:1.0
      respectFlipped:YES
               hints:nil];

    x += rightAlign ? -AutofillPopupLayoutModel::kIconPadding
                    : iconSize.width + AutofillPopupLayoutModel::kIconPadding;
    return x;
}

- (CGFloat)drawSubtext:(NSString*)subtext
                   atX:(CGFloat)x
                 index:(NSInteger)index
            rightAlign:(BOOL)rightAlign
                bounds:(NSRect)bounds
           textYOffset:(CGFloat)textYOffset {
  NSDictionary* subtextAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:controller_->layout_model()
                                       .GetLabelFontListForRow(index)
                                       .GetPrimaryFont()
                                       .GetNativeFont(),
                                   NSFontAttributeName, [self subtextColor],
                                   NSForegroundColorAttributeName, nil];
  NSSize subtextSize = [subtext sizeWithAttributes:subtextAttributes];
  x -= rightAlign ? subtextSize.width : 0;
  CGFloat y = bounds.origin.y + (bounds.size.height - subtextSize.height) / 2;
  y += textYOffset;

  [subtext drawAtPoint:NSMakePoint(x, y) withAttributes:subtextAttributes];
  x += rightAlign ? 0 : subtextSize.width;
  return x;
}

- (NSImage*)iconAtIndex:(NSInteger)index {
  const base::string16& icon = controller_->GetSuggestionAt(index).icon;
  if (icon.empty())
    return nil;
  if (icon == base::ASCIIToUTF16("httpWarning") ||
      icon == base::ASCIIToUTF16("httpsInvalid")) {
    return NSImageFromImageSkiaWithColorSpace(
        controller_->layout_model().GetIconImage(index),
        base::mac::GetSRGBColorSpace());
  }
  int iconId = delegate_->GetIconResourceID(icon);
  DCHECK_NE(-1, iconId);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(iconId).ToNSImage();
}

- (NSColor*)touchBarSubtextColor {
  return [NSColor colorWithCalibratedRed:180.0 / 255.0
                                   green:180.0 / 255.0
                                    blue:180.0 / 255.0
                                   alpha:1.0];
}

- (void)acceptCreditCard:(id)sender {
  ui::LogTouchBarUMA(ui::TouchBarAction::CREDIT_CARD_AUTOFILL);
  controller_->AcceptSuggestion([sender tag]);
}

@end

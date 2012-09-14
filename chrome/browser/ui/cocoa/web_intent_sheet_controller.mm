// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_intent_sheet_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#import "chrome/browser/ui/cocoa/tabs/throbber_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

// The width of a service button, in view coordinates.
const CGFloat kServiceButtonWidth = 300;

// Spacing in between sections.
const CGFloat kVerticalSpacing = 18;

// Square size of the close button.
const CGFloat kCloseButtonSize = 16;

// Font size for picker header.
const CGFloat kHeaderFontSize = 14.5;

// Width of the text fields.
const CGFloat kTextWidth = WebIntentPicker::kWindowWidth -
    (WebIntentPicker::kContentAreaBorder * 2.0 + kCloseButtonSize);

// Maximum number of intents (suggested and installed) displayed.
const int kMaxIntentRows = 4;

// Sets properties on the given |field| to act as title or description labels.
void ConfigureTextFieldAsLabel(NSTextField* field) {
  [field setEditable:NO];
  [field setSelectable:YES];
  [field setDrawsBackground:NO];
  [field setBezeled:NO];
}

NSButton* CreateHyperlinkButton(NSString* title, const NSRect& frame) {
  NSButton* button = [[NSButton alloc] initWithFrame:frame];
  scoped_nsobject<HyperlinkButtonCell> cell(
      [[HyperlinkButtonCell alloc] initTextCell:title]);
  [cell setControlSize:NSSmallControlSize];
  [button setCell:cell.get()];
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];

  return button;
}

}  // namespace

// This simple NSView subclass is used as the single subview of the page info
// bubble's window's contentView. Drawing is flipped so that layout of the
// sections is easier. Apple recommends flipping the coordinate origin when
// doing a lot of text layout because it's more natural.
@interface WebIntentsContentView : NSView
@end
@implementation WebIntentsContentView
- (BOOL)isFlipped {
  return YES;
}

- (void)drawRect:(NSRect)rect {
  [[NSColor colorWithCalibratedWhite:1.0 alpha:1.0] set];
  NSRectFill(rect);
}
@end

// NSImageView subclassed to allow fading the alpha value of the image to
// indicate an inactive/disabled extension.
@interface DimmableImageView : NSImageView {
 @private
  CGFloat alpha;
}

- (void)setEnabled:(BOOL)enabled;

// NSView override
- (void)drawRect:(NSRect)rect;
@end

@implementation DimmableImageView
- (void)drawRect:(NSRect)rect {
  NSImage* image = [self image];

  NSRect sourceRect, destinationRect;
  sourceRect.origin = NSZeroPoint;
  sourceRect.size = [image size];

  destinationRect.origin = NSZeroPoint;
  destinationRect.size = [self frame].size;

  // If the source image is smaller than the destination, center it.
  if (destinationRect.size.width > sourceRect.size.width) {
    destinationRect.origin.x =
        (destinationRect.size.width - sourceRect.size.width) / 2.0;
    destinationRect.size.width = sourceRect.size.width;
  }
  if (destinationRect.size.height > sourceRect.size.height) {
    destinationRect.origin.y =
        (destinationRect.size.height - sourceRect.size.height) / 2.0;
    destinationRect.size.height = sourceRect.size.height;
  }

  [image drawInRect:destinationRect
           fromRect:sourceRect
          operation:NSCompositeSourceOver
           fraction:alpha];
}

- (void)setEnabled:(BOOL)enabled {
  if (enabled)
    alpha = 1.0;
  else
    alpha = 0.5;
  [self setNeedsDisplay:YES];
}
@end

@interface WaitingView : NSView {
 @private
  scoped_nsobject<NSTextField> text_;
  scoped_nsobject<ThrobberView> throbber_;
}
@end

@implementation WaitingView
- (id)init {
  NSRect frame = NSMakeRect(WebIntentPicker::kContentAreaBorder, 0,
                            kTextWidth, 140);

  if (self = [super initWithFrame:frame]) {
    const CGFloat kTopMargin = 35.0;
    const CGFloat kBottomMargin = 25.0;
    const CGFloat kVerticalSpacing = 18.0;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    frame.origin = NSMakePoint(WebIntentPicker::kContentAreaBorder,
                               kBottomMargin);
    frame.size.width = kTextWidth;
    text_.reset([[NSTextField alloc] initWithFrame:frame]);
    ConfigureTextFieldAsLabel(text_);
    [text_ setAlignment:NSCenterTextAlignment];
    [text_ setFont:[NSFont boldSystemFontOfSize:[NSFont systemFontSize]]];
    [text_ setStringValue:
        l10n_util::GetNSStringWithFixup(IDS_INTENT_PICKER_WAIT_FOR_CWS)];
    frame.size.height +=
        [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
              text_];
    [text_ setFrame:frame];

    frame.origin.y += NSHeight(frame) + kVerticalSpacing;

    // The sprite image consists of all the animation frames put together in one
    // horizontal/wide image. Each animation frame is square in shape within the
    // sprite.
    NSImage* iconImage =
        rb.GetNativeImageNamed(IDR_SPEECH_INPUT_SPINNER).ToNSImage();
    frame.size = [iconImage size];
    frame.size.width = NSHeight(frame);
    frame.origin.x = (WebIntentPicker::kWindowWidth - NSWidth(frame))/2.0;
    throbber_.reset([ThrobberView filmstripThrobberViewWithFrame:frame
                                                           image:iconImage]);

    frame.size = NSMakeSize(WebIntentPicker::kWindowWidth,
                            NSMaxY(frame) + kTopMargin);
    frame.origin = NSMakePoint(0, 0);
    [self setSubviews:@[throbber_, text_]];
    [self setFrame:frame];
  }
  return self;
}
@end

// An NSView subclass to display ratings stars.
@interface RatingsView : NSView
  // Mark RatingsView as disabled/enabled.
- (void)setEnabled:(BOOL)enabled;
@end

@implementation RatingsView
- (void)setEnabled:(BOOL)enabled {
  for (DimmableImageView* imageView in [self subviews])
    [imageView setEnabled:enabled];
}
@end

// NSView for the header of the box.
@interface HeaderView : NSView {
 @private
  // Used to forward button clicks. Weak reference.
  scoped_nsobject<NSTextField> titleField_;
  scoped_nsobject<NSTextField> subtitleField_;
  scoped_nsobject<NSBox> spacer_;
}

- (id)init;
@end

@implementation HeaderView
- (id)init {
  NSRect contentFrame = NSMakeRect(0, 0, WebIntentPicker::kWindowWidth, 1);
  if (self = [super initWithFrame:contentFrame]) {
    NSRect frame = NSMakeRect(WebIntentPicker::kContentAreaBorder, 0,
                              kTextWidth, 1);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    titleField_.reset([[NSTextField alloc] initWithFrame:frame]);
    ConfigureTextFieldAsLabel(titleField_);
    gfx::Font titleFont = rb.GetFont(ConstrainedWindow::kTitleFontStyle);
    titleFont = titleFont.DeriveFont(0, gfx::Font::BOLD);
    [titleField_ setFont:titleFont.GetNativeFont()];

    frame = NSMakeRect(WebIntentPicker::kContentAreaBorder, 0,
                       kTextWidth, 1);
    subtitleField_.reset([[NSTextField alloc] initWithFrame:frame]);
    ConfigureTextFieldAsLabel(subtitleField_);
    gfx::Font textFont = rb.GetFont(ConstrainedWindow::kTextFontStyle);
    [subtitleField_ setFont:textFont.GetNativeFont()];

    frame = NSMakeRect(0, 0, WebIntentPicker::kWindowWidth, 1.0);
    spacer_.reset([[NSBox alloc] initWithFrame:frame]);
    [spacer_ setBoxType:NSBoxSeparator];
    [spacer_ setBorderColor:[NSColor blackColor]];

    NSArray* subviews = @[titleField_, subtitleField_, spacer_];
    [self setSubviews:subviews];
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  NSRect frame = [titleField_ frame];
  [titleField_ setStringValue:title];
  frame.size.height +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
            titleField_];
  [titleField_ setFrame:frame];
}

- (void)setSubtitle:(NSString*)subtitle {
  if (subtitle && [subtitle length]) {
    NSRect frame = [subtitleField_ frame];

    [subtitleField_ setHidden:FALSE];
    [subtitleField_ setStringValue:subtitle];
     frame.size.height +=
         [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
               subtitleField_];
     [subtitleField_ setFrame:frame];
  } else {
    [subtitleField_ setHidden:TRUE];
  }
}

- (void)performLayout {
  CGFloat offset = kVerticalSpacing;

  NSRect frame = [spacer_ frame];
  frame.origin.y = offset;
  [spacer_ setFrame:frame];
  offset += NSHeight(frame);

  offset += kVerticalSpacing;

  if (![subtitleField_ isHidden]) {
    frame = [subtitleField_ frame];
    frame.origin.y = offset;
    [subtitleField_ setFrame:frame];
    offset += NSHeight(frame);
  }

  frame = [titleField_ frame];
  frame.origin.y = offset;
  [titleField_ setFrame:frame];
  offset += NSHeight(frame);

  // No kContentAreaBorder here, since that is currently handled elsewhere.
  frame = [self frame];
  frame.size.height = offset;
  [self setFrame:frame];
}
@end

// NSView for a single row in the intents view.
@interface IntentRowView : NSView {
 @private
  scoped_nsobject<NSProgressIndicator> throbber_;
  scoped_nsobject<NSButton> cwsButton_;
  scoped_nsobject<RatingsView> ratingsWidget_;
  scoped_nsobject<NSButton> installButton_;
  scoped_nsobject<DimmableImageView> iconView_;
  scoped_nsobject<NSTextField> label_;
}

- (id)initWithExtension:
    (const WebIntentPickerModel::SuggestedExtension*)extension
              withIndex:(size_t)index
          forController:(WebIntentPickerSheetController*)controller;
- (void)startThrobber;
- (void)setEnabled:(BOOL)enabled;
- (void)stopThrobber;
- (NSInteger)tag;
@end

@implementation IntentRowView
const CGFloat kMaxHeight = 34.0;
const CGFloat kTitleX = 20.0;
const CGFloat kMinAddButtonHeight = 28.0;
const CGFloat kAddButtonX = 245;
const CGFloat kAddButtonWidth = 128.0;

+ (RatingsView*)createStarWidgetWithRating:(CGFloat)rating {
  const int kStarSpacing = 1;  // Spacing between stars in pixels.
  const CGFloat kStarSize = 16.0; // Size of the star in pixels.

  NSMutableArray* subviews = [NSMutableArray array];

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSRect imageFrame = NSMakeRect(0, 0, kStarSize, kStarSize);

  for (int i = 0; i < 5; ++i) {
    NSImage* nsImage = rb.GetNativeImageNamed(
        WebIntentPicker::GetNthStarImageIdFromCWSRating(rating, i)).ToNSImage();

    scoped_nsobject<DimmableImageView> imageView(
        [[DimmableImageView alloc] initWithFrame:imageFrame]);
    [imageView setImage:nsImage];
    [imageView setImageFrameStyle:NSImageFrameNone];
    [imageView setFrame:imageFrame];
    [imageView setEnabled:YES];
    [subviews addObject:imageView];
    imageFrame.origin.x += kStarSize + kStarSpacing;
  }

  NSRect frame = NSMakeRect(0, 0, (kStarSize + kStarSpacing) * 5, kStarSize);
  RatingsView* widget = [[RatingsView alloc] initWithFrame:frame];
  [widget setSubviews:subviews];
  return widget;
}


- (void)setIcon:(NSImage*)icon {
  NSRect imageFrame = NSZeroRect;

  iconView_.reset(
      [[DimmableImageView alloc] initWithFrame:imageFrame]);
  [iconView_ setImage:icon];
  [iconView_ setImageFrameStyle:NSImageFrameNone];
  [iconView_ setEnabled:YES];

  imageFrame.size = [icon size];
  imageFrame.size.height = std::min(NSHeight(imageFrame), kMaxHeight);
  imageFrame.origin.y += (kMaxHeight - NSHeight(imageFrame)) / 2.0;
  [iconView_ setFrame:imageFrame];
}

- (void)setActionButton:(NSString*)title
           withSelector:(SEL)selector
          forController:(WebIntentPickerSheetController*)controller {

  NSRect frame = NSMakeRect(kAddButtonX, 0, kAddButtonWidth, 0);
  installButton_.reset([[NSButton alloc] initWithFrame:frame]);
  [installButton_ setAlignment:NSCenterTextAlignment];
  [installButton_ setButtonType:NSMomentaryPushInButton];
  [installButton_ setBezelStyle:NSRegularSquareBezelStyle];
  [installButton_ setTitle:title];
  frame.size.height = std::min(kMinAddButtonHeight, kMaxHeight);
  frame.origin.y += (kMaxHeight - NSHeight(frame)) / 2.0;
  [installButton_ setFrame:frame];

  [installButton_ setTarget:controller];
  [installButton_ setAction:selector];
}

- (id)init {
  // Build the main view.
  NSRect contentFrame = NSMakeRect(
      0, 0, WebIntentPicker::kWindowWidth, kMaxHeight);
  if (self = [super initWithFrame:contentFrame]) {
    NSMutableArray* subviews = [NSMutableArray array];
    if (iconView_) [subviews addObject:iconView_];
    if (label_) [subviews addObject:label_];
    if (cwsButton_) [subviews addObject:cwsButton_];
    if (ratingsWidget_) [subviews addObject:ratingsWidget_];
    if (installButton_) [subviews addObject:installButton_];
    if (throbber_) [subviews addObject:throbber_];
    [self setSubviews:subviews];
  }
  return self;
}

- (id)initWithExtension:
    (const WebIntentPickerModel::SuggestedExtension*)extension
              withIndex:(size_t)index
          forController:(WebIntentPickerSheetController*)controller {
  // Add the extension icon.
  [self setIcon:extension->icon.ToNSImage()];

  // Add the extension title.
  NSRect frame = NSMakeRect(kTitleX, 0, 0, 0);
  const string16 elidedTitle = ui::ElideText(
      extension->title, gfx::Font(), WebIntentPicker::kTitleLinkMaxWidth,
      ui::ELIDE_AT_END);
  NSString* string = base::SysUTF16ToNSString(elidedTitle);
  cwsButton_.reset(CreateHyperlinkButton(string, frame));
  [cwsButton_ setAlignment:NSCenterTextAlignment];
  [cwsButton_ setTarget:controller];
  [cwsButton_ setAction:@selector(openExtensionLink:)];
  [cwsButton_ setTag:index];
  [cwsButton_ sizeToFit];

  frame = [cwsButton_ frame];
  frame.size.height = std::min([[cwsButton_ cell] cellSize].height,
                               kMaxHeight);
  frame.origin.y = (kMaxHeight - NSHeight(frame)) / 2.0;
  [cwsButton_ setFrame:frame];

  // Add the star rating
  CGFloat offsetX = frame.origin.x + NSWidth(frame) +
      WebIntentPicker::kContentAreaBorder;
  ratingsWidget_.reset(
      [IntentRowView
          createStarWidgetWithRating:extension->average_rating]);
  frame = [ratingsWidget_ frame];
  frame.origin.y += (kMaxHeight - NSHeight(frame)) / 2.0;
  frame.origin.x = offsetX;
  [ratingsWidget_ setFrame:frame];

  // Add an "add to chromium" button.
  string = l10n_util::GetNSStringWithFixup(
      IDS_INTENT_PICKER_INSTALL_EXTENSION);
  [self setActionButton:string
           withSelector:@selector(installExtension:)
          forController:controller];
  [installButton_ setTag:index];

  // Keep a throbber handy.
  frame = [installButton_ frame];
  frame.origin.x += (NSWidth(frame) - 16) / 2;
  frame.origin.y += (NSHeight(frame) - 16) /2;
  frame.size = NSMakeSize(16, 16);
  throbber_.reset([[NSProgressIndicator alloc] initWithFrame:frame]);
  [throbber_ setHidden:YES];
  [throbber_ setStyle:NSProgressIndicatorSpinningStyle];

  return [self init];
}

- (id)initWithService:
    (const WebIntentPickerModel::InstalledService*)service
            withIndex:(size_t)index
        forController:(WebIntentPickerSheetController*)controller {

  // Add the extension icon.
  [self setIcon:service->favicon.ToNSImage()];

  // Add the extension title.
  NSRect frame = NSMakeRect(
      kTitleX, 0, WebIntentPicker::kTitleLinkMaxWidth, 10);

  const string16 elidedTitle = ui::ElideText(
      service->title, gfx::Font(),
      WebIntentPicker::kTitleLinkMaxWidth, ui::ELIDE_AT_END);
  NSString* string = base::SysUTF16ToNSString(elidedTitle);

  label_.reset([[NSTextField alloc] initWithFrame:frame]);
  ConfigureTextFieldAsLabel(label_);
  [label_ setStringValue:string];
  frame.size.height +=
       [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
             label_];
  frame.origin.y = (kMaxHeight - NSHeight(frame)) / 2.0;
  [label_ setFrame:frame];

  [self setActionButton:@"Select"
           withSelector:@selector(invokeService:)
          forController:controller];
  [installButton_ setTag:index];
  return [self init];
}

- (NSInteger)tag {
  return [installButton_ tag];
}

- (void)startThrobber {
  [installButton_ setHidden:YES];
  [throbber_ setHidden:NO];
  [throbber_ startAnimation:self];
  [iconView_ setEnabled:YES];
  [ratingsWidget_ setEnabled:NO];
}

- (void)setEnabled:(BOOL) enabled {
  [installButton_ setEnabled:enabled];
  [cwsButton_ setEnabled:enabled];
  [iconView_ setEnabled:enabled];
  [ratingsWidget_ setEnabled:enabled];
}

- (void)stopThrobber {
  if (throbber_.get()) {
    [throbber_ setHidden:YES];
    [throbber_ stopAnimation:self];
  }
  [installButton_ setHidden:NO];
}

- (void)adjustButtonSize:(CGFloat)newWidth {
  CGFloat increase = std::max(kAddButtonWidth, newWidth) - kAddButtonWidth;
  NSRect frame = [self frame];
  frame.size.width += increase;
  [self setFrame:frame];

  frame = [installButton_ frame];
  frame.size.width += increase;
  [installButton_ setFrame:frame];
}

@end

@interface IntentView : NSView {
 @private
  // Used to forward button clicks. Weak reference.
  WebIntentPickerSheetController* controller_;
}

- (id)initWithModel:(WebIntentPickerModel*)model
      forController:(WebIntentPickerSheetController*)controller;
- (void)startThrobberForRow:(NSInteger)index;
- (void)stopThrobber;
@end

@implementation IntentView
- (id)initWithModel:(WebIntentPickerModel*)model
      forController:(WebIntentPickerSheetController*)controller {
  if (self = [super initWithFrame:NSZeroRect]) {
    const CGFloat kYMargin = 16.0;
    int availableSpots = kMaxIntentRows;

    CGFloat offset = kYMargin;
    NSMutableArray* subviews = [NSMutableArray array];
    NSMutableArray* rows = [NSMutableArray array];

    for (size_t i = 0; i < model->GetInstalledServiceCount() && availableSpots;
         ++i, --availableSpots) {
      const WebIntentPickerModel::InstalledService& svc =
          model->GetInstalledServiceAt(i);
      scoped_nsobject<NSView> suggestView(
          [[IntentRowView alloc] initWithService:&svc
                                       withIndex:i
                                   forController:controller]);

      [rows addObject:suggestView];
    }

    // Special case for the empty view.
    size_t count = model->GetSuggestedExtensionCount();
    if (count == 0 && availableSpots == kMaxIntentRows)
      return nil;

    for (size_t i = count; i > 0 && availableSpots; --i, --availableSpots) {
      const WebIntentPickerModel::SuggestedExtension& ext =
          model->GetSuggestedExtensionAt(i - 1);
      scoped_nsobject<NSView> suggestView(
          [[IntentRowView alloc] initWithExtension:&ext
                                                withIndex:i-1
                                            forController:controller]);
      [rows addObject:suggestView];
    }

    // Determine optimal width for buttons given localized texts.
    scoped_nsobject<NSButton> sizeHelper(
        [[NSButton alloc] initWithFrame:NSZeroRect]);
    [sizeHelper setAlignment:NSCenterTextAlignment];
    [sizeHelper setButtonType:NSMomentaryPushInButton];
    [sizeHelper setBezelStyle:NSRegularSquareBezelStyle];

    [sizeHelper setTitle:
      l10n_util::GetNSStringWithFixup(IDS_INTENT_PICKER_INSTALL_EXTENSION)];
    [sizeHelper sizeToFit];
    CGFloat buttonWidth = NSWidth([sizeHelper frame]);

    [sizeHelper setTitle:
      l10n_util::GetNSStringWithFixup(IDS_INTENT_PICKER_SELECT_INTENT)];
    [sizeHelper sizeToFit];
    buttonWidth = std::max(buttonWidth, NSWidth([sizeHelper frame]));

    for (IntentRowView* row in [rows reverseObjectEnumerator]) {
      [row adjustButtonSize:buttonWidth];
      offset += [self addStackedView:row
                          toSubviews:subviews
                            atOffset:offset];
    }

    [self setSubviews:subviews];
    NSRect contentFrame = NSMakeRect(WebIntentPicker::kContentAreaBorder, 0,
                                     WebIntentPicker::kWindowWidth, offset);
    [self setFrame:contentFrame];
    controller_ = controller;
  }
  return self;
}

- (void)startThrobberForRow:(NSInteger)index {
  for (IntentRowView* row in [self subviews]) {
    if ([row isMemberOfClass:[IntentRowView class]]) {
      [row setEnabled:NO];
      if ([row tag] == index) {
        [row startThrobber];
      }
    }
  }
}

- (void)stopThrobber {
  for (IntentRowView* row in [self subviews]) {
    if ([row isMemberOfClass:[IntentRowView class]]) {
      [row stopThrobber];
      [row setEnabled:YES];
    }
  }
}

- (IBAction)installExtension:(id)sender {
  [controller_ installExtension:sender];
}

- (CGFloat)addStackedView:(NSView*)view
               toSubviews:(NSMutableArray*)subviews
                 atOffset:(CGFloat)offset {
  if (view == nil)
    return 0.0;

  NSPoint frameOrigin = [view frame].origin;
  frameOrigin.y = offset;
  [view setFrameOrigin:frameOrigin];
  [subviews addObject:view];

  return NSHeight([view frame]);
}
@end

@implementation WebIntentPickerSheetController

- (id)initWithPicker:(WebIntentPickerCocoa*)picker {
  // Use an arbitrary height because it will reflect the size of the content.
  NSRect contentRect = NSMakeRect(
      0, 0, WebIntentPicker::kWindowWidth, kVerticalSpacing);

  // |window| is retained by the ConstrainedWindowMacDelegateCustomSheet when
  // the sheet is initialized.
  scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:contentRect
                                  styleMask:NSTitledWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:YES]);
  if ((self = [super initWithWindow:window.get()])) {
    picker_ = picker;
    if (picker)
      model_ = picker->model();

    inlineDispositionTitleField_.reset([[NSTextField alloc] init]);
    ConfigureTextFieldAsLabel(inlineDispositionTitleField_);

    flipView_.reset([[WebIntentsContentView alloc] init]);
    [flipView_ setAutoresizingMask:NSViewMinYMargin];
    [[[self window] contentView] setSubviews:@[flipView_]];

    [self performLayoutWithModel:model_];
  }
  return self;
}

// Handle default OSX dialog cancel mechanisms. (Cmd-.)
- (void)cancelOperation:(id)sender {
  if (picker_)
    picker_->OnCancelled();
  [self closeSheet];
}

- (void)chooseAnotherService:(id)sender {
  if (picker_)
    picker_->OnChooseAnotherService();
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  if (picker_)
    picker_->OnSheetDidEnd(sheet);
}

- (void)setInlineDispositionTabContents:(TabContents*)tabContents {
  contents_ = tabContents;
}

- (void)setInlineDispositionFrameSize:(NSSize)inlineContentSize {
  DCHECK(contents_);

  NSView* webContentView = contents_->web_contents()->GetNativeView();

  // Compute container size to fit all elements, including padding.
  NSSize containerSize = inlineContentSize;
  containerSize.height +=
      [webContentView frame].origin.y + WebIntentPicker::kContentAreaBorder;
  containerSize.width += 2 * WebIntentPicker::kContentAreaBorder;

  // Ensure minimum container width.
  containerSize.width =
      std::max(CGFloat(WebIntentPicker::kWindowWidth), containerSize.width);

  // Resize web contents.
  [webContentView setFrameSize:inlineContentSize];

  // Position close button.
  NSRect buttonFrame = [closeButton_ frame];
  buttonFrame.origin.x = containerSize.width -
      WebIntentPicker::kContentAreaBorder - kCloseButtonSize;
  [closeButton_ setFrame:buttonFrame];

  [self setContainerSize:containerSize];
}

- (void)setContainerSize:(NSSize)containerSize {
  // Resize container views
  NSRect frame = NSMakeRect(0, 0, 0, 0);
  frame.size = containerSize;
  [[[self window] contentView] setFrame:frame];
  [flipView_ setFrame:frame];

  // Resize and reposition dialog window.
  frame.size = [[[self window] contentView] convertSize:containerSize
                                                 toView:nil];
  frame = [[self window] frameRectForContentRect:frame];

  // Readjust window position to keep top in place and center horizontally.
  NSRect windowFrame = [[self window] frame];
  windowFrame.origin.x -= (NSWidth(frame) - NSWidth(windowFrame)) / 2.0;
  windowFrame.origin.y -= (NSHeight(frame) - NSHeight(windowFrame));
  windowFrame.size = frame.size;
  [[self window] setFrame:windowFrame display:YES animate:NO];
}

// Pop up a new tab with the Chrome Web Store.
- (IBAction)showChromeWebStore:(id)sender {
  DCHECK(picker_);
  picker_->OnSuggestionsLinkClicked(
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]));
}

// A picker button has been pressed - invoke corresponding service.
- (IBAction)invokeService:(id)sender {
  DCHECK(picker_);
  picker_->OnServiceChosen([sender tag]);
}

- (IBAction)openExtensionLink:(id)sender {
  DCHECK(model_);
  DCHECK(picker_);
  const WebIntentPickerModel::SuggestedExtension& extension =
      model_->GetSuggestedExtensionAt([sender tag]);

  picker_->OnExtensionLinkClicked(UTF16ToUTF8(extension.id),
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]));
}

- (IBAction)installExtension:(id)sender {
  DCHECK(model_);
  DCHECK(picker_);
  const WebIntentPickerModel::SuggestedExtension& extension =
      model_->GetSuggestedExtensionAt([sender tag]);
  if (picker_) {
    [intentView_ startThrobberForRow:[sender tag]];
    [closeButton_ setEnabled:NO];
    picker_->OnExtensionInstallRequested(UTF16ToUTF8(extension.id));
  }
}

- (CGFloat)addStackedView:(NSView*)view
               toSubviews:(NSMutableArray*)subviews
                 atOffset:(CGFloat)offset {
  if (view == nil)
    return 0.0;

  NSPoint frameOrigin = [view frame].origin;
  frameOrigin.y = offset;
  [view setFrameOrigin:frameOrigin];
  [subviews addObject:view];

  return NSHeight([view frame]);
}

// Adds a link to the Chrome Web Store, to obtain further intent handlers.
// Returns the y position delta for the next offset.
- (CGFloat)addCwsButtonToSubviews:(NSMutableArray*)subviews
                         atOffset:(CGFloat)offset {
  NSRect frame =
      NSMakeRect(WebIntentPicker::kContentAreaBorder, offset, 100, 10);
  NSString* string =
      l10n_util::GetNSStringWithFixup(IDS_FIND_MORE_INTENT_HANDLER_MESSAGE);
  scoped_nsobject<NSButton> button(CreateHyperlinkButton(string,frame));
  [button setTarget:self];
  [button setAction:@selector(showChromeWebStore:)];
  [subviews addObject:button.get()];

  // Call size-to-fit to fixup for the localized string.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button.get()];

  return NSHeight([button frame]);
}

- (void)addCloseButtonToSubviews:(NSMutableArray*)subviews  {
  if (!closeButton_.get()) {
    NSRect buttonFrame = NSMakeRect(
        WebIntentPicker::kContentAreaBorder + kTextWidth,
        WebIntentPicker::kContentAreaBorder,
        kCloseButtonSize, kCloseButtonSize);
    closeButton_.reset(
        [[HoverCloseButton alloc] initWithFrame:buttonFrame]);
    [closeButton_ setTarget:self];
    [closeButton_ setAction:@selector(cancelOperation:)];
    [[closeButton_ cell] setKeyEquivalent:@"\e"];
  }
  [subviews addObject:closeButton_];
}

// Adds a header (icon and explanatory text) to picker bubble.
// Returns the y position delta for the next offset.
- (CGFloat)addHeaderToSubviews:(NSMutableArray*)subviews
                      atOffset:(CGFloat)offset {
  // Create a new text field if we don't have one yet.
  // TODO(groby): This should not be necessary since the controller sends this
  // string.
  if (!actionTextField_.get()) {
    NSString* nsString =
        l10n_util::GetNSStringWithFixup(IDS_CHOOSE_INTENT_HANDLER_MESSAGE);
    [self setActionString:nsString];
  }

  scoped_nsobject<HeaderView> header([[HeaderView alloc] init]);

  [header setTitle:[actionTextField_ stringValue]];
  string16 labelText;
  if (model_ && model_->GetInstalledServiceCount() == 0)
    labelText = model_->GetSuggestionsLinkText();
  [header setSubtitle:base::SysUTF16ToNSString(labelText)];
  [header performLayout];

  NSRect frame = [header frame];
  frame.origin.y = offset;
  [header setFrame:frame];
  [subviews addObject:header];

  return NSHeight(frame);
}

- (CGFloat)addInlineHtmlToSubviews:(NSMutableArray*)subviews
                          atOffset:(CGFloat)offset {
  if (!contents_)
    return 0;

  // Determine a good size for the inline disposition window.
  gfx::Size size = WebIntentPicker::GetMinInlineDispositionSize();
  NSRect frame = NSMakeRect(
      WebIntentPicker::kContentAreaBorder, offset, size.width(), size.height());

  [contents_->web_contents()->GetNativeView() setFrame:frame];
  [subviews addObject:contents_->web_contents()->GetNativeView()];

  return NSHeight(frame);
}

- (CGFloat)addAnotherServiceLinkToSubviews:(NSMutableArray*)subviews
                                  atOffset:(CGFloat)offset {

  NSRect textFrame =
      NSMakeRect(WebIntentPicker::kContentAreaBorder, offset, kTextWidth, 1);
  [inlineDispositionTitleField_ setFrame:textFrame];
  [subviews addObject:inlineDispositionTitleField_];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:inlineDispositionTitleField_];
  textFrame = [inlineDispositionTitleField_ frame];

  // Add link for "choose another service" if other suggestions are available
  // or if more than one (the current) service is installed.
  if (model_->GetInstalledServiceCount() > 1 ||
    model_->GetSuggestedExtensionCount()) {
    NSRect frame = NSMakeRect(
        NSMaxX(textFrame) + WebIntentPicker::kContentAreaBorder, offset,
        WebIntentPicker::kTitleLinkMaxWidth, 1);
    NSString* string = l10n_util::GetNSStringWithFixup(
        IDS_INTENT_PICKER_USE_ALTERNATE_SERVICE);
    scoped_nsobject<NSButton> button(CreateHyperlinkButton(string, frame));
    [[button cell] setControlSize:NSRegularControlSize];
    [button setTarget:self];
    [button setAction:@selector(chooseAnotherService:)];
    [subviews addObject:button];

    // Call size-to-fit to fixup for the localized string.
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:button];

    // And finally, make sure the link and the title are horizontally centered.
    frame = [button frame];
    CGFloat height = std::max(NSHeight(textFrame), NSHeight(frame));
    frame.origin.y += (height - NSHeight(frame)) / 2.0;
    frame.size.height = height;
    textFrame.origin.y += (height - NSHeight(textFrame)) / 2.0;
    textFrame.size.height = height;
    [button setFrame:frame];
    [inlineDispositionTitleField_ setFrame:textFrame];
  }

  return NSHeight(textFrame);
}

- (NSView*)createEmptyView {
  NSRect titleFrame = NSMakeRect(WebIntentPicker::kContentAreaBorder,
                                 WebIntentPicker::kContentAreaBorder,
                                 kTextWidth, 1);
  scoped_nsobject<NSTextField> title(
      [[NSTextField alloc] initWithFrame:titleFrame]);
  ConfigureTextFieldAsLabel(title);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Font titleFont = rb.GetFont(ConstrainedWindow::kTitleFontStyle);
  titleFont = titleFont.DeriveFont(0, gfx::Font::BOLD);
  [title setFont:titleFont.GetNativeFont()];
  [title setStringValue:
      l10n_util::GetNSStringWithFixup(IDS_INTENT_PICKER_NO_SERVICES_TITLE)];
  titleFrame.size.height +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:title];

  NSRect bodyFrame = titleFrame;
  bodyFrame.origin.y +=
      NSHeight(titleFrame) + WebIntentPicker::kContentAreaBorder;

  scoped_nsobject<NSTextField> body(
      [[NSTextField alloc] initWithFrame:bodyFrame]);
  ConfigureTextFieldAsLabel(body);
  [body setStringValue:
      l10n_util::GetNSStringWithFixup(IDS_INTENT_PICKER_NO_SERVICES)];
  bodyFrame.size.height +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:body];
  NSRect viewFrame = NSMakeRect(
      0,
      WebIntentPicker::kContentAreaBorder,
      std::max(NSWidth(bodyFrame), NSWidth(titleFrame)) +
          2 * WebIntentPicker::kContentAreaBorder,
      NSHeight(titleFrame) + NSHeight(bodyFrame) + kVerticalSpacing);

  titleFrame.origin.y = NSHeight(viewFrame) - NSHeight(titleFrame);
  bodyFrame.origin.y = 0;

  [title setFrame:titleFrame];
  [body setFrame:bodyFrame];

  NSView* view = [[NSView alloc] initWithFrame:viewFrame];
  [view setSubviews:@[title, body]];

  return view;
}

- (void)performLayoutWithModel:(WebIntentPickerModel*)model {
  model_ = model;

  // |offset| is the Y position that should be drawn at next.
  CGFloat offset = WebIntentPicker::kContentAreaBorder;

  // Keep the new subviews in an array that gets replaced at the end.
  NSMutableArray* subviews = [NSMutableArray array];

  // Indicator that we have neither suggested nor installed services,
  // and we're not in the wait stage any more either.
  BOOL isEmpty = model_ &&
      !model_->IsWaitingForSuggestions() &&
      !model_->GetInstalledServiceCount() &&
      !model_->GetSuggestedExtensionCount();

  if (model_ && model_->IsWaitingForSuggestions()) {
    if (!waitingView_.get())
      waitingView_.reset([[WaitingView alloc] init]);
    [subviews addObject:waitingView_];
    offset += NSHeight([waitingView_ frame]);
  } else if (isEmpty) {
    scoped_nsobject<NSView> emptyView([self createEmptyView]);
    [subviews addObject:emptyView];
    offset += NSHeight([emptyView frame]);
  } else if (contents_) {
    offset += [self addAnotherServiceLinkToSubviews:subviews
                                           atOffset:offset];
    offset += WebIntentPicker::kContentAreaBorder;
    offset += [self addInlineHtmlToSubviews:subviews atOffset:offset];
  } else {
    offset += [self addHeaderToSubviews:subviews atOffset:offset];

    if (model) {
      intentView_.reset(
          [[IntentView alloc] initWithModel:model forController:self]);
      offset += [self addStackedView:intentView_
                          toSubviews:subviews
                            atOffset:offset];
    }
    offset += [self addCwsButtonToSubviews:subviews atOffset:offset];
  }
  [self addCloseButtonToSubviews:subviews];

  // Add the bottom padding.
  offset += WebIntentPicker::kContentAreaBorder;

  // Replace the window's content.
  [flipView_ setSubviews:subviews];

  // And resize to fit.
  [self setContainerSize:NSMakeSize(WebIntentPicker::kWindowWidth, offset)];
}

- (void)setActionString:(NSString*)actionString {
  NSRect textFrame;
  if (!actionTextField_.get()) {
    textFrame = NSMakeRect(WebIntentPicker::kContentAreaBorder, 0,
                           kTextWidth, 1);

    actionTextField_.reset([[NSTextField alloc] initWithFrame:textFrame]);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    ConfigureTextFieldAsLabel(actionTextField_);
    gfx::Font titleFont = rb.GetFont(ConstrainedWindow::kTitleFontStyle);
    titleFont = titleFont.DeriveFont(0, gfx::Font::BOLD);
    [actionTextField_ setFont:titleFont.GetNativeFont()];
  } else {
    textFrame = [actionTextField_ frame];
  }

  [actionTextField_ setStringValue:actionString];
  textFrame.size.height +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
            actionTextField_];
  [actionTextField_ setFrame:textFrame];
}

- (void)setInlineDispositionTitle:(NSString*)title {
  NSFont* nsfont = [inlineDispositionTitleField_ font];
  gfx::Font font(
      base::SysNSStringToUTF8([nsfont fontName]), [nsfont pointSize]);
  NSString* elidedTitle = base::SysUTF16ToNSString(ui::ElideText(
        base::SysNSStringToUTF16(title),
        font, WebIntentPicker::kTitleLinkMaxWidth, ui::ELIDE_AT_END));
  [inlineDispositionTitleField_ setStringValue:elidedTitle];
}

- (void)stopThrobber {
  [closeButton_ setEnabled:YES];
  [intentView_ stopThrobber];
}

- (void)closeSheet {
  [NSApp endSheet:[self window]];
}

@end  // WebIntentPickerSheetController

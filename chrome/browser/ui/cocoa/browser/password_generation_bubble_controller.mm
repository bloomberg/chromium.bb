// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/password_generation_bubble_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#import "chrome/browser/ui/cocoa/styled_text_field_cell.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "content/public/browser/render_view_host.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/tracking_area.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"

namespace {

// Size of the border in the bubble.
const CGFloat kBorderSize = 9.0;

// Visible size of the textfield.
const CGFloat kTextFieldHeight = 20.0;
const CGFloat kTextFieldWidth = 172.0;

// Frame padding necessary to make the textfield the correct visible size.
const CGFloat kTextFieldTopPadding = 3.0;

// Visible size of the button
const CGFloat kButtonWidth = 63.0;
const CGFloat kButtonHeight = 20.0;

// Padding that is added to the frame around the button to make it the
// correct visible size. Determined via visual inspection.
const CGFloat kButtonHorizontalPadding = 6.0;
const CGFloat kButtonVerticalPadding = 3.0;

// Visible size of the title.
const CGFloat kTitleWidth = 170.0;
const CGFloat kTitleHeight = 15.0;

// Space between the title and the textfield.
const CGFloat kVerticalSpacing = 13.0;

// Space between the textfield and the button.
const CGFloat kHorizontalSpacing = 7.0;

// We don't actually want the border to be kBorderSize on top as there is
// whitespace in the title text that makes it looks substantially bigger.
const CGFloat kTopBorderOffset = 3.0;

const CGFloat kIconSize = 26.0;

}  // namespace

// Customized StyledTextFieldCell to display one button decoration that changes
// on hover.
@interface PasswordGenerationTextFieldCell : StyledTextFieldCell {
 @private
  PasswordGenerationBubbleController* controller_;
  BOOL hovering_;
  base::scoped_nsobject<NSImage> normalImage_;
  base::scoped_nsobject<NSImage> hoverImage_;
}

- (void)setUpWithController:(PasswordGenerationBubbleController*)controller
                normalImage:(NSImage*)normalImage
                 hoverImage:(NSImage*)hoverImage;
- (void)mouseEntered:(NSEvent*)theEvent
              inView:(PasswordGenerationTextField*)controlView;
- (void)mouseExited:(NSEvent*)theEvent
             inView:(PasswordGenerationTextField*)controlView;
- (BOOL)mouseDown:(NSEvent*)theEvent
           inView:(PasswordGenerationTextField*)controlView;
- (void)setUpTrackingAreaInRect:(NSRect)frame
                         ofView:(PasswordGenerationTextField*)controlView;
// Exposed for testing.
- (void)iconClicked;
@end

@implementation PasswordGenerationTextField

+ (Class)cellClass {
  return [PasswordGenerationTextFieldCell class];
}

- (PasswordGenerationTextFieldCell*)cell {
  return base::mac::ObjCCastStrict<PasswordGenerationTextFieldCell>(
      [super cell]);
}

- (id)initWithFrame:(NSRect)frame
     withController:(PasswordGenerationBubbleController*)controller
        normalImage:(NSImage*)normalImage
         hoverImage:(NSImage*)hoverImage {
  self = [super initWithFrame:frame];
  if (self) {
    PasswordGenerationTextFieldCell* cell = [self cell];
    [cell setUpWithController:controller
                  normalImage:normalImage
                   hoverImage:hoverImage];
    [cell setUpTrackingAreaInRect:[self bounds] ofView:self];
  }
  return self;
}

- (void)mouseEntered:(NSEvent*)theEvent {
  [[self cell] mouseEntered:theEvent inView:self];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [[self cell] mouseExited:theEvent inView:self];
}

- (void)mouseDown:(NSEvent*)theEvent {
  // Let the cell handle the click if it's in the decoration.
  if (![[self cell] mouseDown:theEvent inView:self]) {
    if ([self currentEditor]) {
      [[self currentEditor] mouseDown:theEvent];
    } else {
      // We somehow lost focus.
      [super mouseDown:theEvent];
    }
  }
}

- (void)simulateIconClick {
  [[self cell] iconClicked];
}

@end

@implementation PasswordGenerationTextFieldCell

- (void)setUpWithController:(PasswordGenerationBubbleController*)controller
                normalImage:(NSImage*)normalImage
                 hoverImage:(NSImage*)hoverImage {
  controller_ = controller;
  hovering_ = NO;
  normalImage_.reset([normalImage retain]);
  hoverImage_.reset([hoverImage retain]);
  [self setLineBreakMode:NSLineBreakByTruncatingTail];
  [self setTruncatesLastVisibleLine:YES];
}

- (void)splitFrame:(NSRect*)cellFrame toIconFrame:(NSRect*)iconFrame {
  NSDivideRect(*cellFrame, iconFrame, cellFrame,
               kIconSize, NSMaxXEdge);
}

- (NSRect)getIconFrame:(NSRect)cellFrame {
  NSRect iconFrame;
  [self splitFrame:&cellFrame toIconFrame:&iconFrame];
  return iconFrame;
}

- (NSRect)getTextFrame:(NSRect)cellFrame {
  NSRect iconFrame;
  [self splitFrame:&cellFrame toIconFrame:&iconFrame];
  return cellFrame;
}

- (BOOL)eventIsInDecoration:(NSEvent*)theEvent
                     inView:(PasswordGenerationTextField*)controlView {
  NSPoint mouseLocation = [controlView convertPoint:[theEvent locationInWindow]
                                           fromView:nil];
  NSRect cellFrame = [controlView bounds];
  return NSMouseInRect(mouseLocation,
                       [self getIconFrame:cellFrame],
                       [controlView isFlipped]);
}

- (void)mouseEntered:(NSEvent*)theEvent
              inView:(PasswordGenerationTextField*)controlView {
  hovering_ = YES;
  [controlView setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)theEvent
             inView:(PasswordGenerationTextField*)controlView {
  hovering_ = NO;
  [controlView setNeedsDisplay:YES];
}

- (BOOL)mouseDown:(NSEvent*)theEvent
           inView:(PasswordGenerationTextField*)controlView {
  if ([self eventIsInDecoration:theEvent inView:controlView]) {
    [self iconClicked];
    return YES;
  }
  return NO;
}

- (void)iconClicked {
  [controller_ regeneratePassword];
}

- (NSImage*)getImage {
  if (hovering_)
    return hoverImage_;
  return normalImage_;
}

- (NSRect)adjustFrameForFrame:(NSRect)frame {
  // By default, there appears to be a 2 pixel gap between what is considered
  // part of the textFrame and what is considered part of the icon.
  // TODO(gcasto): This really should be fixed in StyledTextFieldCell, as it
  // looks like the location bar also suffers from this issue.
  frame.size.width += 2;
  return frame;
}

- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  // Baseclass insets the rect by top and bottom offsets.
  NSRect textFrame = [super textFrameForFrame:cellFrame];
  textFrame = [self getTextFrame:textFrame];
  return [self adjustFrameForFrame:textFrame];
}

- (NSRect)textCursorFrameForFrame:(NSRect)cellFrame {
  NSRect textFrame = [self getTextFrame:cellFrame];
  return [self adjustFrameForFrame:textFrame];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  NSImage* image = [self getImage];
  NSRect iconFrame = [self getIconFrame:cellFrame];
  // Center the image in the available space. At the moment the image is
  // slightly larger than the frame so we crop it.
  // Offset the full difference on the left hand side since the border on the
  // right takes up some space. Offset half the vertical difference on the
  // bottom so that the image stays vertically centered.
  const CGFloat xOffset = [image size].width - NSWidth(iconFrame);
  const CGFloat yOffset = ([image size].height - (NSHeight(iconFrame))) / 2.0;
  NSRect croppedRect = NSMakeRect(xOffset,
                                  yOffset,
                                  NSWidth(iconFrame),
                                  NSHeight(iconFrame));

  [image drawInRect:iconFrame
           fromRect:croppedRect
          operation:NSCompositeSourceOver
           fraction:1.0
     respectFlipped:YES
              hints:nil];

  [super drawInteriorWithFrame:cellFrame inView:controlView];
}

- (void)setUpTrackingAreaInRect:(NSRect)frame
                         ofView:(PasswordGenerationTextField*)view {
  NSRect iconFrame = [self getIconFrame:frame];
  base::scoped_nsobject<CrTrackingArea> area(
      [[CrTrackingArea alloc] initWithRect:iconFrame
                                   options:NSTrackingMouseEnteredAndExited |
          NSTrackingActiveAlways owner:view userInfo:nil]);
  [view addTrackingArea:area];
}

- (CGFloat)topTextFrameOffset {
  return 1.0;
}

- (CGFloat)bottomTextFrameOffset {
  return 1.0;
}

- (CGFloat)cornerRadius {
  return 4.0;
}

- (BOOL)shouldDrawBezel {
  return YES;
}

@end

@implementation PasswordGenerationBubbleController

@synthesize textField = textField_;

- (id)initWithWindow:(NSWindow*)parentWindow
          anchoredAt:(NSPoint)point
      renderViewHost:(content::RenderViewHost*)renderViewHost
     passwordManager:(password_manager::PasswordManager*)passwordManager
      usingGenerator:(autofill::PasswordGenerator*)passwordGenerator
             forForm:(const autofill::PasswordForm&)form {
  CGFloat width = (kBorderSize*2 +
                   kTextFieldWidth +
                   kHorizontalSpacing +
                   kButtonWidth);
  CGFloat height = (kBorderSize*2 +
                    kTextFieldHeight +
                    kVerticalSpacing +
                    kTitleHeight -
                    kTopBorderOffset +
                    info_bubble::kBubbleArrowHeight);
  NSRect contentRect = NSMakeRect(0, 0, width, height);
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  if (self = [super initWithWindow:window
                      parentWindow:parentWindow
                        anchoredAt:point]) {
    passwordGenerator_ = passwordGenerator;
    renderViewHost_ = renderViewHost;
    passwordManager_ = passwordManager;
    form_ = form;
    [[self bubble] setArrowLocation:info_bubble::kTopLeft];
    [self performLayout];
  }

  return self;
}

- (void)performLayout {
  NSView* contentView = [[self window] contentView];
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  textField_ = [[[PasswordGenerationTextField alloc]
      initWithFrame:NSMakeRect(kBorderSize,
                               kBorderSize,
                               kTextFieldWidth,
                               kTextFieldHeight + kTextFieldTopPadding)
     withController:self
        normalImage:rb.GetNativeImageNamed(IDR_RELOAD_DIMMED).ToNSImage()
         hoverImage:rb.GetNativeImageNamed(IDR_RELOAD)
             .ToNSImage()] autorelease];
  const gfx::FontList& smallBoldFont =
      rb.GetFontList(ui::ResourceBundle::SmallBoldFont);
  [textField_ setFont:smallBoldFont.GetPrimaryFont().GetNativeFont()];
  [textField_
    setStringValue:base::SysUTF8ToNSString(passwordGenerator_->Generate())];
  [textField_ setDelegate:self];
  [contentView addSubview:textField_];

  CGFloat buttonX = (NSMaxX([textField_ frame]) +
                     kHorizontalSpacing -
                     kButtonHorizontalPadding);
  CGFloat buttonY = kBorderSize - kButtonVerticalPadding;
  NSButton* button =
      [[NSButton alloc] initWithFrame:NSMakeRect(
            buttonX,
            buttonY,
            kButtonWidth + 2 * kButtonHorizontalPadding,
            kButtonHeight + 2 * kButtonVerticalPadding)];
  [button setBezelStyle:NSRoundedBezelStyle];
  [button setTitle:l10n_util::GetNSString(IDS_PASSWORD_GENERATION_BUTTON_TEXT)];
  [button setTarget:self];
  [button setAction:@selector(fillPassword:)];
  [contentView addSubview:button];

  base::scoped_nsobject<NSTextField> title([[NSTextField alloc] initWithFrame:
          NSMakeRect(kBorderSize,
                     kBorderSize + kTextFieldHeight + kVerticalSpacing,
                     kTitleWidth,
                     kTitleHeight)]);
  [title setEditable:NO];
  [title setBordered:NO];
  [title setStringValue:l10n_util::GetNSString(
        IDS_PASSWORD_GENERATION_BUBBLE_TITLE)];
  [contentView addSubview:title];
}

- (IBAction)fillPassword:(id)sender {
  if (renderViewHost_) {
    renderViewHost_->Send(
        new AutofillMsg_GeneratedPasswordAccepted(
            renderViewHost_->GetRoutingID(),
            base::SysNSStringToUTF16([textField_ stringValue])));
  }
  if (passwordManager_)
    passwordManager_->SetFormHasGeneratedPassword(form_);

  actions_.password_accepted = true;
  [self close];
}

- (void)regeneratePassword {
  [textField_
    setStringValue:base::SysUTF8ToNSString(passwordGenerator_->Generate())];
  actions_.password_regenerated = true;
}

- (void)controlTextDidChange:(NSNotification*)notification {
  actions_.password_edited = true;
}

- (void)windowWillClose:(NSNotification*)notification {
  autofill::password_generation::LogUserActions(actions_);
  [super windowWillClose:notification];
}

@end

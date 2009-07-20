// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"  // for NOTREACHED()
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/infobar.h"
#import "chrome/browser/cocoa/infobar_container_controller.h"
#import "chrome/browser/cocoa/infobar_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "skia/ext/skia_utils_mac.h"
#include "webkit/glue/window_open_disposition.h"


@interface InfoBarController (PrivateMethods)
// Closes the infobar by calling RemoveDelegate on the container.
// This will remove the infobar from its associated TabContents as
// well as trigger the deletion of this InfoBarController.  Once the
// delegate is removed from the container, it is no longer needed, so
// we ask it to delete itself.
- (void)closeInfoBar;
@end

@implementation InfoBarController

@synthesize containerController = containerController_;
@synthesize delegate = delegate_;

- (id)initWithDelegate:(InfoBarDelegate*)delegate {
  DCHECK(delegate);
  if ((self = [super initWithNibName:@"InfoBar"
                              bundle:mac_util::MainAppBundle()])) {
    delegate_ = delegate;
  }
  return self;
}

// All infobars have an icon, so we set up the icon in the base class
// awakeFromNib.
- (void)awakeFromNib {
  if (delegate_->GetIcon()) {
    [image_ setImage:gfx::SkBitmapToNSImage(*(delegate_->GetIcon()))];
  }

  [self addAdditionalControls];
}

// Called when someone clicks on the ok button.
- (void)ok:(id)sender {
  // Subclasses must override this method if they do not hide the ok button.
  NOTREACHED();
}

// Called when someone clicks on the cancel button.
- (void)cancel:(id)sender {
  // Subclasses must override this method if they do not hide the cancel button.
  NOTREACHED();
}

// Called when someone clicks on the close button.
- (void)dismiss:(id)sender {
  [self closeInfoBar];
}

- (void)addAdditionalControls {
  // Default implementation does nothing.
}

@end

@implementation InfoBarController (PrivateMethods)
- (void)closeInfoBar {
  // Calling RemoveDelegate() triggers notifications which will remove
  // the infobar view from the infobar container.  At that point it is
  // safe to ask the delegate to delete itself.
  DCHECK(delegate_);
  [containerController_ removeDelegate:delegate_];
  delegate_->InfoBarClosed();
  delegate_ = NULL;
}
@end


/////////////////////////////////////////////////////////////////////////
// AlertInfoBarController implementation

@implementation AlertInfoBarController

// Alert infobars have a text message.
- (void)addAdditionalControls {
  AlertInfoBarDelegate* delegate = delegate_->AsAlertInfoBarDelegate();
  [label_ setStringValue:base::SysWideToNSString(
        delegate->GetMessageText())];

  // Remove the ok and cancel buttons, since they are not needed.
  [okButton_ removeFromSuperview];
  [cancelButton_ removeFromSuperview];
}

@end


/////////////////////////////////////////////////////////////////////////
// LinkInfoBarController implementation

@implementation LinkInfoBarController

// Link infobars have a text message, of which part is linkified.  We
// use an NSAttributedString to display styled text, and we set a
// NSLink attribute on the hyperlink portion of the message.  Infobars
// use a custom NSTextField subclass, which allows us to override
// textView:clickedOnLink:atIndex: and intercept clicks.
//
// TODO(rohitrao): Using an NSTextField here has some weird UI side
// effects, such as showing the wrong cursor at times.  Explore other
// solutions.
- (void)addAdditionalControls {
  LinkInfoBarDelegate* delegate = delegate_->AsLinkInfoBarDelegate();
  size_t offset = std::wstring::npos;
  std::wstring message = delegate->GetMessageTextWithOffset(&offset);

  // Create an attributes dictionary for the entire message.  We have
  // to expicitly set the font to the system font, because
  // NSAttributedString defaults to Helvetica 12.  We also override
  // the cursor to give us the normal cursor rather than the text
  // insertion cursor.
  NSMutableDictionary* linkAttributes =
      [NSMutableDictionary dictionaryWithObject:[NSCursor arrowCursor]
                           forKey:NSCursorAttributeName];
  [linkAttributes setObject:[NSFont systemFontOfSize:[NSFont systemFontSize]]
                  forKey:NSFontAttributeName];

  // Create the attributed string for the main message text.
  NSMutableAttributedString* infoText =
      [[[NSMutableAttributedString alloc]
         initWithString:base::SysWideToNSString(message)] autorelease];
  [infoText addAttributes:linkAttributes
                    range:NSMakeRange(0, [infoText length])];

  // Add additional attributes to style the link text appropriately as
  // well as linkify it.  We use an empty string for the NSLink
  // attribute because the actual object we pass doesn't matter, but
  // it cannot be nil.
  [linkAttributes setObject:[NSColor blueColor]
                     forKey:NSForegroundColorAttributeName];
  [linkAttributes setObject:[NSNumber numberWithBool:YES]
                     forKey:NSUnderlineStyleAttributeName];
  [linkAttributes setObject:[NSCursor pointingHandCursor]
                     forKey:NSCursorAttributeName];
  [linkAttributes setObject:[NSString string]  // dummy value
                     forKey:NSLinkAttributeName];

  // Insert the link text into the string at the appropriate offset.
  [infoText insertAttributedString:
              [[[NSAttributedString alloc]
                 initWithString:base::SysWideToNSString(delegate->GetLinkText())
                     attributes:linkAttributes] autorelease]
            atIndex:offset];

  // Update the label view with the new text.  The view must be
  // selectable and allow editing text attributes for the
  // linkification to work correctly.
  [label_ setAllowsEditingTextAttributes: YES];
  [label_ setSelectable: YES];
  [label_ setAttributedStringValue:infoText];

  // Remove the ok and cancel buttons, since they are not needed.
  [okButton_ removeFromSuperview];
  [cancelButton_ removeFromSuperview];
}

// Called when someone clicks on the link in the infobar.  This method
// is called by the InfobarTextField on its delegate (the
// LinkInfoBarController).
- (void)linkClicked {
  // TODO(rohitrao): Set the disposition correctly based on modifier keys.
  WindowOpenDisposition disposition = CURRENT_TAB;
  if (delegate_->AsLinkInfoBarDelegate()->LinkClicked(disposition))
    [self closeInfoBar];
}

@end


/////////////////////////////////////////////////////////////////////////
// ConfirmInfoBarController implementation

@implementation ConfirmInfoBarController

// Called when someone clicks on the "OK" button.
- (IBAction)ok:(id)sender {
  if (delegate_->AsConfirmInfoBarDelegate()->Accept())
    [self closeInfoBar];
}

// Called when someone clicks on the "Cancel" button.
- (IBAction)cancel:(id)sender {
  if (delegate_->AsConfirmInfoBarDelegate()->Cancel())
    [self closeInfoBar];
}

// Confirm infobars can have OK and/or cancel buttons, depending on
// the return value of GetButtons().  We create each button if
// required and position them to the left of the close button.
- (void)addAdditionalControls {
  ConfirmInfoBarDelegate* delegate = delegate_->AsConfirmInfoBarDelegate();
  int visibleButtons = delegate->GetButtons();
  [label_ setStringValue:base::SysWideToNSString(delegate->GetMessageText())];

  // Save the margins between the buttons, so we can keep them constant.
  float cancelMargin =
      NSMinX([closeButton_ frame]) - NSMaxX([cancelButton_ frame]);
  float okMargin = NSMinX([cancelButton_ frame]) - NSMaxX([okButton_ frame]);
  float labelMargin = NSMinX([okButton_ frame]) - NSMaxX([label_ frame]);

  // Create and position the cancel button if needed.  Otherwise, hide it.
  if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    [cancelButton_ setTitle:base::SysWideToNSString(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL))];
    [cancelButton_ sizeToFit];

    NSRect cancelFrame = [cancelButton_ frame];
    float cancelWidth = cancelFrame.size.width + cancelMargin;

    // Position the cancel button to the left of the close button.
    // The appropriate margin is already built into cancelWidth.
    cancelFrame.origin.x = NSMinX([closeButton_ frame]) - cancelWidth;
    [cancelButton_ setFrame:cancelFrame];

    // Resize the label box to extend all the way to the cancel button,
    // minus the saved margin, but only if we're not also adding an OK button.
    if (!(visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK)) {
      NSRect labelFrame = [label_ frame];
      labelFrame.size.width =
          NSMinX(cancelFrame) - NSMinX(labelFrame) - labelMargin;
      [label_ setFrame:labelFrame];
    }
  } else {
    [cancelButton_ removeFromSuperview];
  }

  // Create and position the OK button if needed.  Otherwise, hide it.
  if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK) {
    [okButton_ setTitle:base::SysWideToNSString(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK))];
    [okButton_ sizeToFit];

    NSRect okFrame = [okButton_ frame];
    int okWidth = okFrame.size.width + okMargin;

    // Position the OK button to the left of the cancel button, if
    // present.  Otherwise, position it relative to the close button.
    float relativeX = (visibleButtons & ConfirmInfoBarDelegate::BUTTON_CANCEL) ?
        NSMinX([cancelButton_ frame]) :
        NSMinX([closeButton_ frame]);

    // The appropriate margin is already built into okWidth.
    okFrame.origin.x = relativeX - okWidth;
    [okButton_ setFrame:okFrame];

    // Resize the label box to extend all the way to the OK button,
    // minus the saved margin.
    NSRect labelFrame = [label_ frame];
    labelFrame.size.width = NSMinX(okFrame) - NSMinX(labelFrame) - labelMargin;
    [label_ setFrame:labelFrame];
  } else {
    [okButton_ removeFromSuperview];
  }
}

@end


//////////////////////////////////////////////////////////////////////////
// CreateInfoBar() implementations

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  AlertInfoBarController* controller =
      [[AlertInfoBarController alloc] initWithDelegate:this];
  return new InfoBar(controller);
}

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  LinkInfoBarController* controller =
      [[LinkInfoBarController alloc] initWithDelegate:this];
  return new InfoBar(controller);
}

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  ConfirmInfoBarController* controller =
      [[ConfirmInfoBarController alloc] initWithDelegate:this];
  return new InfoBar(controller);
}

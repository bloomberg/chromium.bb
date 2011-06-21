// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"

@class AnimatableView;
@class HoverCloseButton;
@protocol InfoBarContainer;
class InfoBarDelegate;
@class InfoBarGradientView;

// A controller for an infobar in the browser window.  There is one
// controller per infobar view.  The base InfoBarController is able to
// draw an icon, a text message, and a close button.  Subclasses can
// override addAdditionalControls to customize the UI.
@interface InfoBarController : NSViewController<NSTextViewDelegate> {
 @private
  id<InfoBarContainer> containerController_;  // weak, owns us
  BOOL infoBarClosing_;

 @protected
  IBOutlet InfoBarGradientView* infoBarView_;
  IBOutlet NSImageView* image_;
  IBOutlet NSTextField* labelPlaceholder_;
  IBOutlet NSButton* okButton_;
  IBOutlet NSButton* cancelButton_;
  IBOutlet HoverCloseButton* closeButton_;

  // In rare instances, it can be possible for |delegate_| to delete itself
  // while this controller is still alive.  Always check |delegate_| against
  // NULL before using it.
  InfoBarDelegate* delegate_;  // weak, can be NULL

  // Text fields don't work as well with embedded links as text views, but
  // text views cannot conveniently be created in IB. The xib file contains
  // a text field |labelPlaceholder_| that's replaced by this text view |label_|
  // in -awakeFromNib.
  scoped_nsobject<NSTextView> label_;
};

// Initializes a new InfoBarController.
- (id)initWithDelegate:(InfoBarDelegate*)delegate;

// Called when someone clicks on the OK or Cancel buttons.  Subclasses
// must override if they do not hide the buttons.
- (void)ok:(id)sender;
- (void)cancel:(id)sender;

// Called when someone clicks on the close button.  Dismisses the
// infobar without taking any action.
- (IBAction)dismiss:(id)sender;

// Returns a pointer to this controller's view, cast as an AnimatableView.
- (AnimatableView*)animatableView;

// Open or animate open the infobar.
- (void)open;
- (void)animateOpen;

// Close or animate close the infobar.
- (void)close;
- (void)animateClosed;

// Subclasses can override this method to add additional controls to
// the infobar view.  This method is called by awakeFromNib.  The
// default implementation does nothing.
- (void)addAdditionalControls;

// Subclasses must override this method to perform cleanup just before the
// infobar closes.
- (void)infobarWillClose;

// Sets the info bar message to the specified |message|.
- (void)setLabelToMessage:(NSString*)message;

// Removes the OK and Cancel buttons and resizes the textfield to use the
// space.
- (void)removeButtons;

@property(nonatomic, assign) id<InfoBarContainer> containerController;
@property(nonatomic, readonly) InfoBarDelegate* delegate;

@end

/////////////////////////////////////////////////////////////////////////
// InfoBarController subclasses, one for each InfoBarDelegate
// subclass.  Each of these subclasses overrides addAdditionalControls to
// configure its view as necessary.

@interface LinkInfoBarController : InfoBarController
// Called when there is a click on the link in the infobar.
- (void)linkClicked;
@end


@interface ConfirmInfoBarController : InfoBarController
// Called when the OK and Cancel buttons are clicked.
- (IBAction)ok:(id)sender;
- (IBAction)cancel:(id)sender;
// Called when there is a click on the link in the infobar.
- (void)linkClicked;
@end

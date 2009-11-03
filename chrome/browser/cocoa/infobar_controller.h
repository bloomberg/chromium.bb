// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@class AnimatableView;
@protocol InfoBarContainer;
class InfoBarDelegate;

// A controller for an infobar in the browser window.  There is one
// controller per infobar view.  The base InfoBarController is able to
// draw an icon, a text message, and a close button.  Subclasses can
// override addAdditionalControls to customize the UI.
@interface InfoBarController : NSViewController {
 @private
  id<InfoBarContainer> containerController_;  // weak, owns us
  BOOL infoBarClosing_;

 @protected
  InfoBarDelegate* delegate_;  // weak
  IBOutlet NSView* infoBarView_;
  IBOutlet NSImageView* image_;
  IBOutlet NSTextField* label_;
  IBOutlet NSButton* okButton_;
  IBOutlet NSButton* cancelButton_;
};

// Initializes a new InfoBarController.
- (id)initWithDelegate:(InfoBarDelegate*)delegate;

// Called when someone clicks on the ok or cancel buttons.  Subclasses
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

@property(assign, nonatomic) id<InfoBarContainer> containerController;
@property(readonly) InfoBarDelegate* delegate;

@end

/////////////////////////////////////////////////////////////////////////
// InfoBarController subclasses, one for each InfoBarDelegate
// subclass.  Each of these subclasses overrides addAdditionalControls to
// configure its view as necessary.

@interface AlertInfoBarController : InfoBarController
@end


@interface LinkInfoBarController : InfoBarController
// Called when there is a click on the link in the infobar.
- (void)linkClicked;
@end


@interface ConfirmInfoBarController : InfoBarController
// Called when the ok and cancel buttons are clicked.
- (IBAction)ok:(id)sender;
- (IBAction)cancel:(id)sender;
@end

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "InstallerWindowController.h"

#import "AppDelegate.h"

@interface InstallerWindowController () {
  NSButton* importButton_;
  NSButton* defaultBrowserButton_;
  NSButton* optInButton_;
  NSButton* launchButton_;
  NSTextField* statusDescription_;
  NSTextField* downloadProgressDescription_;
  NSProgressIndicator* progressBar_;
}
@end

@implementation InstallerWindowController

// Most buttons have the same style and differ only by their title, this method
// simplifies styling the buttons and provides an argument for the title.
- (void)stylizeButton:(NSButton*)button withTitle:(NSString*)title {
  button.buttonType = NSSwitchButton;
  button.bezelStyle = NSRoundedBezelStyle;
  button.title = title;
}

// Similar to stylizeButton except works with NSTextField objects instead.
- (void)stylizeTextField:(NSTextField*)textField
         withDescription:(NSString*)description {
  textField.backgroundColor = NSColor.clearColor;
  textField.textColor = NSColor.blackColor;
  textField.stringValue = description;
  textField.bezeled = NO;
  textField.editable = NO;
}

// Positions and stylizes buttons.
- (void)setUpButtons {
  importButton_ = [[NSButton alloc] initWithFrame:NSMakeRect(30, 20, 300, 25)];
  [self stylizeButton:importButton_
            withTitle:@"Import from... Wait import what?"];

  defaultBrowserButton_.state = NSOnState;
  defaultBrowserButton_ =
      [[NSButton alloc] initWithFrame:NSMakeRect(30, 45, 300, 25)];
  [self stylizeButton:defaultBrowserButton_
            withTitle:@"Make Chrome the default browser."];

  optInButton_ = [[NSButton alloc] initWithFrame:NSMakeRect(30, 70, 300, 25)];
  [self stylizeButton:optInButton_ withTitle:@"Say yes to UMA."];

  launchButton_ = [[NSButton alloc] initWithFrame:NSMakeRect(310, 6, 100, 50)];
  launchButton_.buttonType = NSPushOnPushOffButton;
  launchButton_.bezelStyle = NSRoundedBezelStyle;
  launchButton_.title = @"Launch";
  [launchButton_ setEnabled:NO];
  [launchButton_ setAction:@selector(launchButtonClicked)];
}

// Positions and stylizes textfields.
- (void)setUpTextfields {
  statusDescription_ =
      [[NSTextField alloc] initWithFrame:NSMakeRect(20, 95, 300, 20)];
  [self stylizeTextField:statusDescription_
         withDescription:@"Working on it! While you're waiting..."];

  downloadProgressDescription_ =
      [[NSTextField alloc] initWithFrame:NSMakeRect(20, 160, 300, 20)];
  [self stylizeTextField:downloadProgressDescription_
         withDescription:@"Downloading... "];
}

// Positions and stylizes the progressbar for download and install.
- (void)setUpProgressBar {
  progressBar_ =
      [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(15, 125, 400, 50)];
  progressBar_.indeterminate = NO;
  progressBar_.style = NSProgressIndicatorBarStyle;
  progressBar_.maxValue = 100.0;
  progressBar_.minValue = 0.0;
  progressBar_.doubleValue = 0.0;
}

// Positions and adds the rest of the UI elements to main window. Prevents
// resizing the window so that the absolute position will look the same on all
// computers. Window is hidden until all positioning is finished.
- (id)initWithWindow:(NSWindow*)window {
  if (self = [super initWithWindow:window]) {
    [window setFrame:NSMakeRect(0, 0, 430, 220) display:YES];
    [window center];
    [window setStyleMask:[window styleMask] & ~NSResizableWindowMask];

    [self setUpButtons];
    [self setUpProgressBar];
    [self setUpTextfields];

    [window.contentView addSubview:importButton_];
    [window.contentView addSubview:defaultBrowserButton_];
    [window.contentView addSubview:optInButton_];
    [window.contentView addSubview:launchButton_];
    [window.contentView addSubview:progressBar_];
    [window.contentView addSubview:statusDescription_];
    [window.contentView addSubview:downloadProgressDescription_];
    [NSApp activateIgnoringOtherApps:YES];
    [window makeKeyAndOrderFront:self];
  }
  return self;
}

- (void)updateStatusDescription:(NSString*)text {
  // First setStringValue statement is required to clear the original string.
  // Omitting the first line will cause occasional ghosting of the previous
  // string.
  // TODO: Find a real solution to the ghosting problem.
  //  downloadProgressDescription_.stringValue = @"";
  downloadProgressDescription_.stringValue = text;
}

- (void)updateDownloadProgress:(double)progressPercent {
  progressBar_.doubleValue = progressPercent;
}

- (void)enableLaunchButton {
  [launchButton_ setEnabled:YES];
}

- (void)launchButtonClicked {
  // TODO: Launch the app and start ejecting disk.
  [NSApp terminate:nil];
}

@end

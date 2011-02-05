// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/instant_confirm_window_controller.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/options/show_options_url.h"
#include "googleurl/src/gurl.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/gfx/native_widget_types.h"

namespace browser {

void ShowInstantConfirmDialog(gfx::NativeWindow parent, Profile* profile) {
  InstantConfirmWindowController* controller =
      [[InstantConfirmWindowController alloc] initWithProfile:profile];
  [NSApp beginSheet:[controller window]
     modalForWindow:parent
      modalDelegate:nil
     didEndSelector:NULL
        contextInfo:NULL];
}

}  // namespace browser

@implementation InstantConfirmWindowController

- (id)initWithProfile:(Profile*)profile {
  NSString* nibPath = [base::mac::MainAppBundle()
                        pathForResource:@"InstantConfirm"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    profile_ = profile;
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);

  CGFloat delta = [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
      description_];
  NSRect descriptionFrame = [description_ frame];
  descriptionFrame.origin.y -= delta;
  [description_ setFrame:descriptionFrame];

  NSRect frame = [[self window] frame];
  frame.size.height += delta;
  [[self window] setFrame:frame display:YES];
}

- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

- (IBAction)learnMore:(id)sender {
  browser::ShowOptionsURL(profile_, browser::InstantLearnMoreURL());
}

- (IBAction)ok:(id)sender {
  InstantController::Enable(profile_);
  [self cancel:sender];
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window]];
  [[self window] close];
}

@end

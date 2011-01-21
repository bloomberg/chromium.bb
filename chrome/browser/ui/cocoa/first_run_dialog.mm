// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/first_run_dialog.h"

#include "base/mac/mac_util.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/locale_settings.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface FirstRunDialogController (PrivateMethods)
// Show the dialog.
- (void)show;
@end

namespace {

// Compare function for -[NSArray sortedArrayUsingFunction:context:] that
// sorts the views in Y order bottom up.
NSInteger CompareFrameY(id view1, id view2, void* context) {
  CGFloat y1 = NSMinY([view1 frame]);
  CGFloat y2 = NSMinY([view2 frame]);
  if (y1 < y2)
    return NSOrderedAscending;
  else if (y1 > y2)
    return NSOrderedDescending;
  else
    return NSOrderedSame;
}

class FirstRunShowBridge : public base::RefCounted<FirstRunShowBridge> {
 public:
  FirstRunShowBridge(FirstRunDialogController* controller);

  void ShowDialog();
 private:
  FirstRunDialogController* controller_;
};

FirstRunShowBridge::FirstRunShowBridge(
    FirstRunDialogController* controller) : controller_(controller) {
}

void FirstRunShowBridge::ShowDialog() {
  [controller_ show];
  MessageLoop::current()->QuitNow();
}

};

@implementation FirstRunDialogController

@synthesize statsEnabled = statsEnabled_;
@synthesize makeDefaultBrowser = makeDefaultBrowser_;

- (id)init {
  NSString* nibpath =
      [base::mac::MainAppBundle() pathForResource:@"FirstRunDialog"
                                          ofType:@"nib"];
  self = [super initWithWindowNibPath:nibpath owner:self];
  if (self != nil) {
    // Bound to the dialog checkbox, default to true.
    makeDefaultBrowser_ = YES;
  }
  return self;
}

- (void)dealloc {
  [super dealloc];
}

- (IBAction)showWindow:(id)sender {
  // The main MessageLoop has not yet run, but has been spun. If we call
  // -[NSApplication runModalForWindow:] we will hang <http://crbug.com/54248>.
  // Therefore the main MessageLoop is run so things work.

  scoped_refptr<FirstRunShowBridge> bridge(new FirstRunShowBridge(self));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(bridge.get(),
                        &FirstRunShowBridge::ShowDialog));
  MessageLoop::current()->Run();
}

- (void)show {
  NSWindow* win = [self window];

  // Only support the sizing the window once.
  DCHECK(!beenSized_) << "ShowWindow was called twice?";
  if (!beenSized_) {
    beenSized_ = YES;
    DCHECK_GT([objectsToSize_ count], 0U);

    // Size everything to fit, collecting the widest growth needed (XIB provides
    // the min size, i.e.-never shrink, just grow).
    CGFloat largestWidthChange = 0.0;
    for (NSView* view in objectsToSize_) {
      DCHECK_NE(statsCheckbox_, view) << "Stats checkbox shouldn't be in list";
      if (![view isHidden]) {
        NSSize delta = [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
        DCHECK_EQ(delta.height, 0.0)
            << "Didn't expect anything to change heights";
        if (largestWidthChange < delta.width)
          largestWidthChange = delta.width;
      }
    }

    // Make the window wide enough to fit everything.
    if (largestWidthChange > 0.0) {
      NSView* contentView = [win contentView];
      NSRect windowFrame = [contentView convertRect:[win frame] fromView:nil];
      windowFrame.size.width += largestWidthChange;
      windowFrame = [contentView convertRect:windowFrame toView:nil];
      [win setFrame:windowFrame display:NO];
    }

    // The stats checkbox gets some really long text, so it gets word wrapped
    // and then sized.
    DCHECK(statsCheckbox_);
    CGFloat statsCheckboxHeightChange = 0.0;
    [GTMUILocalizerAndLayoutTweaker wrapButtonTitleForWidth:statsCheckbox_];
    statsCheckboxHeightChange =
        [GTMUILocalizerAndLayoutTweaker sizeToFitView:statsCheckbox_].height;

    // Walk bottom up shuffling for all the hidden views.
    NSArray* subViews =
        [[[win contentView] subviews] sortedArrayUsingFunction:CompareFrameY
                                                       context:NULL];
    CGFloat moveDown = 0.0;
    NSUInteger numSubViews = [subViews count];
    for (NSUInteger idx = 0 ; idx < numSubViews ; ++idx) {
      NSView* view = [subViews objectAtIndex:idx];

      // If the view is hidden, collect the amount to move everything above it
      // down, if it's not hidden, apply any shift down.
      if ([view isHidden]) {
        DCHECK_GT((numSubViews - 1), idx)
            << "Don't support top view being hidden";
        NSView* nextView = [subViews objectAtIndex:(idx + 1)];
        CGFloat viewBottom = [view frame].origin.y;
        CGFloat nextViewBottom = [nextView frame].origin.y;
        moveDown += nextViewBottom - viewBottom;
      } else {
        if (moveDown != 0.0) {
          NSPoint origin = [view frame].origin;
          origin.y -= moveDown;
          [view setFrameOrigin:origin];
        }
      }
      // Special case, if this is the stats checkbox, everything above it needs
      // to get moved up by the amount it changed height.
      if (view == statsCheckbox_) {
        moveDown -= statsCheckboxHeightChange;
      }
    }

    // Resize the window for any height change from hidden views, etc.
    if (moveDown != 0.0) {
      NSView* contentView = [win contentView];
      [contentView setAutoresizesSubviews:NO];
      NSRect windowFrame = [contentView convertRect:[win frame] fromView:nil];
      windowFrame.size.height -= moveDown;
      windowFrame = [contentView convertRect:windowFrame toView:nil];
      [win setFrame:windowFrame display:NO];
      [contentView setAutoresizesSubviews:YES];
    }

  }

  // Neat weirdness in the below code - the Application menu stays enabled
  // while the window is open but selecting items from it (e.g. Quit) has
  // no effect.  I'm guessing that this is an artifact of us being a
  // background-only application at this stage and displaying a modal
  // window.

  // Display dialog.
  [win center];
  [NSApp runModalForWindow:win];
}

- (IBAction)ok:(id)sender {
  [[self window] close];
  [NSApp stopModal];
}

- (IBAction)learnMore:(id)sender {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kLearnMoreReportingURL));
  NSString* urlStr = base::SysUTF8ToNSString(url.spec());;
  NSURL* learnMoreUrl = [NSURL URLWithString:urlStr];
  [[NSWorkspace sharedWorkspace] openURL:learnMoreUrl];
}

@end

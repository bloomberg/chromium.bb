// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_view.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

SadTab* SadTab::Create(content::WebContents* web_contents, SadTabKind kind) {
  return new SadTabCocoa(web_contents);
}

SadTabCocoa::SadTabCocoa(content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

SadTabCocoa::~SadTabCocoa() {
}

void SadTabCocoa::Show() {
  sad_tab_controller_.reset(
      [[SadTabController alloc] initWithWebContents:web_contents_]);
}

void SadTabCocoa::Close() {
  [[sad_tab_controller_ view] removeFromSuperview];
}

}  // namespace chrome

@implementation SadTabController

- (id)initWithWebContents:(content::WebContents*)webContents {
  if ((self = [super initWithNibName:@"SadTab"
                              bundle:base::mac::FrameworkBundle()])) {
    webContents_ = webContents;

    if (webContents_) {  // NULL in unit_tests.
      NSView* ns_view = webContents_->GetNativeView();
      [[self view] setAutoresizingMask:
          (NSViewWidthSizable | NSViewHeightSizable)];
      [ns_view addSubview:[self view]];
      [[self view] setFrame:[ns_view bounds]];
    }
  }

  return self;
}

- (void)awakeFromNib {
  // If webContents_ is nil, ask view to remove link.
  if (!webContents_) {
    SadTabView* sad_view = static_cast<SadTabView*>([self view]);
    [sad_view removeHelpText];
  }
}

- (content::WebContents*)webContents {
  return webContents_;
}

- (void)openLearnMoreAboutCrashLink:(id)sender {
  // Send the action up through the responder chain.
  [NSApp sendAction:@selector(openLearnMoreAboutCrashLink:) to:nil from:self];
}

@end

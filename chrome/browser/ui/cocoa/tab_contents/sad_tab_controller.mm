// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"

#include "base/mac/bundle_locations.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_view_cocoa.h"
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
  if ((self = [super init])) {
    DCHECK(webContents);
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

- (void)dealloc {
  [[sadTabView_ reloadButton] setTarget:nil];
  [super dealloc];
}

- (void)loadView {
  sadTabView_.reset([[SadTabView alloc] init]);
  [[sadTabView_ reloadButton] setTarget:self];
  [[sadTabView_ reloadButton] setAction:@selector(reloadPage:)];
  [self setView:sadTabView_];
}

- (content::WebContents*)webContents {
  return webContents_;
}

- (IBAction)reloadPage:(id)sender {
  webContents_->GetController().Reload(true);
}

@end

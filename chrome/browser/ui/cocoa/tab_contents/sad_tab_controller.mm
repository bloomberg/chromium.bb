// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"

#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_view_cocoa.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"

using content::OpenURLParams;
using content::Referrer;

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

@interface SadTabController ()<SadTabViewDelegate>
@end

@implementation SadTabController {
  content::WebContents* webContents_;
  SadTabView* sadTabView_;
}

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
  sadTabView_.delegate = nil;
  [super dealloc];
}

- (void)loadView {
  sadTabView_ = [[SadTabView new] autorelease];
  sadTabView_.delegate = self;

  [sadTabView_ setTitle:IDS_SAD_TAB_TITLE];
  [sadTabView_ setMessage:IDS_SAD_TAB_MESSAGE];
  [sadTabView_ setButtonTitle:IDS_SAD_TAB_RELOAD_LABEL];
  [sadTabView_ setHelpLinkTitle:IDS_SAD_TAB_LEARN_MORE_LINK
                            URL:@(chrome::kCrashReasonURL)];

  self.view = sadTabView_;
}

- (content::WebContents*)webContents {
  return webContents_;
}

- (void)sadTabViewButtonClicked:(SadTabView*)sadTabView {
  webContents_->GetController().Reload(true);
}

- (void)sadTabView:(SadTabView*)sadTabView
    helpLinkClickedWithURL:(NSString*)url {
  OpenURLParams params(GURL(url.UTF8String), Referrer(), CURRENT_TAB,
                       ui::PAGE_TRANSITION_LINK, false);
  webContents_->OpenURL(params);
}

@end

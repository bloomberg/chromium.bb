// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hung_renderer_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#import "chrome/browser/ui/cocoa/multi_key_equivalent_button.h"
#import "chrome/browser/ui/cocoa/tab_contents/favicon_util_mac.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

namespace {
// We only support showing one of these at a time per app.  The
// controller owns itself and is released when its window is closed.
HungRendererController* g_instance = NULL;
}  // namespace

class HungRendererWebContentsObserverBridge
    : public content::WebContentsObserver {
 public:
  HungRendererWebContentsObserverBridge(WebContents* web_contents,
                                        HungRendererController* controller)
    : content::WebContentsObserver(web_contents),
      controller_(controller) {
  }

 protected:
  // WebContentsObserver overrides:
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE {
    [controller_ renderProcessGone];
  }
  virtual void WebContentsDestroyed() OVERRIDE {
    [controller_ renderProcessGone];
  }

 private:
  HungRendererController* controller_;  // weak

  DISALLOW_COPY_AND_ASSIGN(HungRendererWebContentsObserverBridge);
};

@implementation HungRendererController

- (id)initWithWindowNibName:(NSString*)nibName {
  NSString* nibpath = [base::mac::FrameworkBundle() pathForResource:nibName
                                                             ofType:@"nib"];
  self = [super initWithWindowNibPath:nibpath owner:self];
  if (self) {
    [tableView_ setDataSource:self];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!g_instance);
  [tableView_ setDataSource:nil];
  [tableView_ setDelegate:nil];
  [killButton_ setTarget:nil];
  [waitButton_ setTarget:nil];
  [super dealloc];
}

- (void)awakeFromNib {
  // Load in the image
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* backgroundImage =
      rb.GetNativeImageNamed(IDR_FROZEN_TAB_ICON).ToNSImage();
  [imageView_ setImage:backgroundImage];

  // Make the message fit.
  CGFloat messageShift =
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:messageView_];

  // Move the graphic up to be top even with the message.
  NSRect graphicFrame = [imageView_ frame];
  graphicFrame.origin.y += messageShift;
  [imageView_ setFrame:graphicFrame];

  // Make the window taller to fit everything.
  NSSize windowDelta = NSMakeSize(0, messageShift);
  [GTMUILocalizerAndLayoutTweaker
      resizeWindowWithoutAutoResizingSubViews:[self window]
                                        delta:windowDelta];

  // Make the "wait" button respond to additional keys.  By setting this to
  // @"\e", it will respond to both Esc and Command-. (period).
  KeyEquivalentAndModifierMask key;
  key.charCode = @"\e";
  [waitButton_ addKeyEquivalent:key];
}

- (IBAction)kill:(id)sender {
  if (hungContents_)
    base::KillProcess(hungContents_->GetRenderProcessHost()->GetHandle(),
                      content::RESULT_CODE_HUNG, false);
  // Cannot call performClose:, because the close button is disabled.
  [self close];
}

- (IBAction)wait:(id)sender {
  if (hungContents_ && hungContents_->GetRenderViewHost())
    hungContents_->GetRenderViewHost()->RestartHangMonitorTimeout();
  // Cannot call performClose:, because the close button is disabled.
  [self close];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView {
  return [hungTitles_ count];
}

- (id)tableView:(NSTableView*)aTableView
      objectValueForTableColumn:(NSTableColumn*)column
            row:(NSInteger)rowIndex {
  return [NSNumber numberWithInt:NSOffState];
}

- (NSCell*)tableView:(NSTableView*)tableView
    dataCellForTableColumn:(NSTableColumn*)tableColumn
                       row:(NSInteger)rowIndex {
  NSCell* cell = [tableColumn dataCellForRow:rowIndex];

  if ([[tableColumn identifier] isEqualToString:@"title"]) {
    DCHECK([cell isKindOfClass:[NSButtonCell class]]);
    NSButtonCell* buttonCell = static_cast<NSButtonCell*>(cell);
    [buttonCell setTitle:[hungTitles_ objectAtIndex:rowIndex]];
    [buttonCell setImage:[hungFavicons_ objectAtIndex:rowIndex]];
    [buttonCell setRefusesFirstResponder:YES];  // Don't push in like a button.
    [buttonCell setHighlightsBy:NSNoCellMask];
  }
  return cell;
}

- (void)windowWillClose:(NSNotification*)notification {
  // We have to reset g_instance before autoreleasing the window,
  // because we want to avoid reusing the same dialog if someone calls
  // chrome::ShowHungRendererDialog() between the autorelease call and the
  // actual dealloc.
  g_instance = nil;

  // Prevent kills from happening after close if the user had the
  // button depressed just when new activity was detected.
  hungContents_ = NULL;

  [self autorelease];
}

// TODO(shess): This could observe all of the tabs referenced in the
// loop, updating the dialog and keeping it up so long as any remain.
// Tabs closed by their renderer will close the dialog (that's
// activity!), so it would not add much value.  Also, the views
// implementation only monitors the initiating tab.
- (void)showForWebContents:(WebContents*)contents {
  DCHECK(contents);
  hungContents_ = contents;
  hungContentsObserver_.reset(
      new HungRendererWebContentsObserverBridge(contents, self));
  base::scoped_nsobject<NSMutableArray> titles([[NSMutableArray alloc] init]);
  base::scoped_nsobject<NSMutableArray> favicons([[NSMutableArray alloc] init]);
  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (it->GetRenderProcessHost() == hungContents_->GetRenderProcessHost()) {
      base::string16 title = it->GetTitle();
      if (title.empty())
        title = CoreTabHelper::GetDefaultTitle();
      [titles addObject:base::SysUTF16ToNSString(title)];
      [favicons addObject:mac::FaviconForWebContents(*it)];
    }
  }
  hungTitles_.reset([titles copy]);
  hungFavicons_.reset([favicons copy]);
  [tableView_ reloadData];

  [[self window] center];
  [self showWindow:self];
}

- (void)endForWebContents:(WebContents*)contents {
  DCHECK(contents);
  DCHECK(hungContents_);
  if (hungContents_ && hungContents_->GetRenderProcessHost() ==
      contents->GetRenderProcessHost()) {
    // Cannot call performClose:, because the close button is disabled.
    [self close];
  }
}

- (void)renderProcessGone {
  // Cannot call performClose:, because the close button is disabled.
  [self close];
}

@end

@implementation HungRendererController (JustForTesting)
- (NSButton*)killButton {
  return killButton_;
}

- (MultiKeyEquivalentButton*)waitButton {
  return waitButton_;
}
@end

namespace chrome {

void ShowHungRendererDialog(WebContents* contents) {
  if (!logging::DialogsAreSuppressed()) {
    if (!g_instance)
      g_instance = [[HungRendererController alloc]
                     initWithWindowNibName:@"HungRendererDialog"];
    [g_instance showForWebContents:contents];
  }
}

void HideHungRendererDialog(WebContents* contents) {
  if (!logging::DialogsAreSuppressed() && g_instance)
    [g_instance endForWebContents:contents];
}

}  // namespace chrome

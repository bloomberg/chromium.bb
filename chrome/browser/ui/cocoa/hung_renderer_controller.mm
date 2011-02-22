// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hung_renderer_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/process_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/hung_renderer_dialog.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/ui/cocoa/multi_key_equivalent_button.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/result_codes.h"
#include "grit/chromium_strings.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image.h"

namespace {
// We only support showing one of these at a time per app.  The
// controller owns itself and is released when its window is closed.
HungRendererController* g_instance = NULL;
}  // end namespace

@implementation HungRendererController

- (id)initWithWindowNibName:(NSString*)nibName {
  NSString* nibpath = [base::mac::MainAppBundle() pathForResource:nibName
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
  [super dealloc];
}

- (void)awakeFromNib {
  // Load in the image
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* backgroundImage = rb.GetNativeImageNamed(IDR_FROZEN_TAB_ICON);
  DCHECK(backgroundImage);
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
                      ResultCodes::HUNG, false);
  // Cannot call performClose:, because the close button is disabled.
  [self close];
}

- (IBAction)wait:(id)sender {
  if (hungContents_ && hungContents_->render_view_host())
    hungContents_->render_view_host()->RestartHangMonitorTimeout();
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
  // hung_renderer_dialog::ShowForTabContents() between the autorelease
  // call and the actual dealloc.
  g_instance = nil;

  [self autorelease];
}

- (void)showForTabContents:(TabContents*)contents {
  DCHECK(contents);
  hungContents_ = contents;
  scoped_nsobject<NSMutableArray> titles([[NSMutableArray alloc] init]);
  scoped_nsobject<NSMutableArray> favicons([[NSMutableArray alloc] init]);
  for (TabContentsIterator it; !it.done(); ++it) {
    if (it->GetRenderProcessHost() == hungContents_->GetRenderProcessHost()) {
      string16 title = (*it)->GetTitle();
      if (title.empty())
        title = TabContentsWrapper::GetDefaultTitle();
      [titles addObject:base::SysUTF16ToNSString(title)];

      // TabContents can return a null SkBitmap if it has no favicon.  If this
      // happens, use the default favicon.
      const SkBitmap& bitmap = it->GetFavIcon();
      if (!bitmap.isNull()) {
        [favicons addObject:gfx::SkBitmapToNSImage(bitmap)];
      } else {
        ResourceBundle& rb = ResourceBundle::GetSharedInstance();
        [favicons addObject:rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON)];
      }
    }
  }
  hungTitles_.reset([titles copy]);
  hungFavicons_.reset([favicons copy]);
  [tableView_ reloadData];

  [[self window] center];
  [self showWindow:self];
}

- (void)endForTabContents:(TabContents*)contents {
  DCHECK(contents);
  DCHECK(hungContents_);
  if (hungContents_ && hungContents_->GetRenderProcessHost() ==
      contents->GetRenderProcessHost()) {
    // Cannot call performClose:, because the close button is disabled.
    [self close];
  }
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

namespace hung_renderer_dialog {

void ShowForTabContents(TabContents* contents) {
  if (!logging::DialogsAreSuppressed()) {
    if (!g_instance)
      g_instance = [[HungRendererController alloc]
                     initWithWindowNibName:@"HungRendererDialog"];
    [g_instance showForTabContents:contents];
  }
}

// static
void HideForTabContents(TabContents* contents) {
  if (!logging::DialogsAreSuppressed() && g_instance)
    [g_instance endForTabContents:contents];
}

}  // namespace hung_renderer_dialog

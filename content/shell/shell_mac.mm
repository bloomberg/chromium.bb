// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include "base/logging.h"
#import "base/mac/cocoa_protocols.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "content/shell/resource.h"
#include "googleurl/src/gurl.h"

// Receives notification that the window is closing so that it can start the
// tear-down process. Is responsible for deleting itself when done.
@interface ContentShellWindowDelegate : NSObject<NSWindowDelegate> {
 @private
  content::Shell* shell_;
}
- (id)initWithShell:(content::Shell*)shell;
@end

@implementation ContentShellWindowDelegate

- (id)initWithShell:(content::Shell*)shell {
  if ((self = [super init])) {
    shell_ = shell;
  }
  return self;
}

// Called when the window is about to close. Perform the self-destruction
// sequence by getting rid of the shell and removing it and the window from
// the various global lists. Instead of doing it here, however, we fire off
// a delayed call to |-cleanup:| to allow everything to get off the stack
// before we go deleting objects. By returning YES, we allow the window to be
// removed from the screen.
- (BOOL)windowShouldClose:(id)window {
  [window autorelease];

  // Clean ourselves up and do the work after clearing the stack of anything
  // that might have the shell on it.
  [self performSelectorOnMainThread:@selector(cleanup:)
                         withObject:window
                      waitUntilDone:NO];

  return YES;
}

// Does the work of removing the window from our various bookkeeping lists
// and gets rid of the shell.
- (void)cleanup:(id)window {
  delete shell_;

  [self release];
}

- (void)performAction:(id)sender {
  shell_->ActionPerformed([sender tag]);
}

- (void)takeURLStringValueFrom:(id)sender {
  shell_->URLEntered(base::SysNSStringToUTF8([sender stringValue]));
}

@end

namespace {

NSString* kWindowTitle = @"Content Shell";

const int kButtonWidth = 72;
const int kURLBarHeight = 24;

void MakeShellButton(NSRect* rect,
                     NSString* title,
                     NSView* parent,
                     int control,
                     NSView* target) {
  NSButton* button = [[[NSButton alloc] initWithFrame:*rect] autorelease];
  [button setTitle:title];
  [button setBezelStyle:NSSmallSquareBezelStyle];
  [button setAutoresizingMask:(NSViewMaxXMargin | NSViewMinYMargin)];
  [button setTarget:target];
  [button setAction:@selector(performAction:)];
  [button setTag:control];
  [parent addSubview:button];
  rect->origin.x += kButtonWidth;
}

}  // namespace

namespace content {

void Shell::PlatformInitialize() {
}

base::StringPiece Shell::PlatformResourceProvider(int key) {
  return base::StringPiece();
}

void Shell::PlatformCleanUp() {
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  int id;
  switch (control) {
    case BACK_BUTTON:
      id = IDC_NAV_BACK;
      break;
    case FORWARD_BUTTON:
      id = IDC_NAV_FORWARD;
      break;
    case STOP_BUTTON:
      id = IDC_NAV_STOP;
      break;
    default:
      NOTREACHED() << "Unknown UI control";
      return;
  }
  [[[window_ contentView] viewWithTag:id] setEnabled:is_enabled];
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  [url_edit_view_ setStringValue:url_string];
}

void Shell::PlatformCreateWindow(int width, int height) {
  NSRect initial_window_bounds = NSMakeRect(0, 0, width, height);
  window_ = [[NSWindow alloc] initWithContentRect:initial_window_bounds
                                        styleMask:(NSTitledWindowMask |
                                                   NSClosableWindowMask |
                                                   NSMiniaturizableWindowMask |
                                                   NSResizableWindowMask )
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  [window_ setTitle:kWindowTitle];

  // Rely on the window delegate to clean us up rather than immediately
  // releasing when the window gets closed. We use the delegate to do
  // everything from the autorelease pool so the shell isn't on the stack
  // during cleanup (ie, a window close from javascript).
  [window_ setReleasedWhenClosed:NO];

  // Create a window delegate to watch for when it's asked to go away. It will
  // clean itself up so we don't need to hold a reference.
  ContentShellWindowDelegate* delegate =
      [[ContentShellWindowDelegate alloc] initWithShell:this];
  [window_ setDelegate:delegate];

  NSRect button_frame =
      NSMakeRect(0, NSMaxY(initial_window_bounds) - kURLBarHeight,
                 kButtonWidth, kURLBarHeight);

  NSView* content = [window_ contentView];

  MakeShellButton(&button_frame, @"Back", content, IDC_NAV_BACK,
                  (NSView*)delegate);
  MakeShellButton(&button_frame, @"Forward", content, IDC_NAV_FORWARD,
                  (NSView*)delegate);
  MakeShellButton(&button_frame, @"Reload", content, IDC_NAV_RELOAD,
                  (NSView*)delegate);
  MakeShellButton(&button_frame, @"Stop", content, IDC_NAV_STOP,
                  (NSView*)delegate);

  button_frame.size.width =
      NSWidth(initial_window_bounds) - NSMinX(button_frame);
  url_edit_view_ =
      [[[NSTextField alloc] initWithFrame:button_frame] autorelease];
  [content addSubview:url_edit_view_];
  [url_edit_view_ setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
  [url_edit_view_ setTarget:delegate];
  [url_edit_view_ setAction:@selector(takeURLStringValueFrom:)];
  [[url_edit_view_ cell] setWraps:NO];
  [[url_edit_view_ cell] setScrollable:YES];

  // show the window
  [window_ makeKeyAndOrderFront:nil];
}

void Shell::PlatformSetContents() {
  // TODO(erg): I don't know what goes here.
}

void Shell::PlatformSizeTo(int width, int height) {
  NSRect frame = [window_ frame];
  frame.size = NSMakeSize(width, height);
  [window_ setFrame:frame display:YES];
}

void Shell::PlatformResizeSubViews() {
  // Not needed; subviews are bound.
}

void Shell::ActionPerformed(int control) {
  switch (control) {
    case IDC_NAV_BACK:
      GoBackOrForward(-1);
      break;
    case IDC_NAV_FORWARD:
      GoBackOrForward(1);
      break;
    case IDC_NAV_RELOAD:
      Reload();
      break;
    case IDC_NAV_STOP:
      Stop();
      break;
  }
}

void Shell::URLEntered(std::string url_string) {
  if (!url_string.empty()) {
    GURL url(url_string);
    if (!url.has_scheme())
      url = GURL("http://" + url_string);
    LoadURL(url);
  }
}

}  // namespace content

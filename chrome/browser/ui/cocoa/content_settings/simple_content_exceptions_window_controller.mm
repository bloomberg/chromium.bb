// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/simple_content_exceptions_window_controller.h"

#include "base/logging.h"
#import "base/mac/mac_util.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/table_model_observer.h"

@interface SimpleContentExceptionsWindowController (Private)
- (id)initWithTableModel:(RemoveRowsTableModel*)model;
@end

namespace  {

const CGFloat kButtonBarHeight = 35.0;

SimpleContentExceptionsWindowController* g_exceptionWindow = nil;

}  // namespace

@implementation SimpleContentExceptionsWindowController

+ (id)controllerWithTableModel:(RemoveRowsTableModel*)model {
  if (!g_exceptionWindow) {
    g_exceptionWindow = [[SimpleContentExceptionsWindowController alloc]
        initWithTableModel:model];
  }
  return g_exceptionWindow;
}

- (id)initWithTableModel:(RemoveRowsTableModel*)model {
  NSString* nibpath = [base::mac::MainAppBundle()
      pathForResource:@"SimpleContentExceptionsWindow"
               ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    model_.reset(model);

    // TODO(thakis): autoremember window rect.
    // TODO(thakis): sorting support.
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);
  DCHECK(tableView_);
  DCHECK(arrayController_);

  CGFloat minWidth = [[removeButton_ superview] bounds].size.width +
                     [[doneButton_ superview] bounds].size.width;
  [[self window] setMinSize:NSMakeSize(minWidth,
                                       [[self window] minSize].height)];
  NSDictionary* columns = [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithInt:IDS_EXCEPTIONS_HOSTNAME_HEADER], @"hostname",
      [NSNumber numberWithInt:IDS_EXCEPTIONS_ACTION_HEADER], @"action",
      nil];
  [arrayController_ bindToTableModel:model_.get()
                         withColumns:columns
                    groupTitleColumn:@"hostname"];
}

- (void)setMinWidth:(CGFloat)minWidth {
  NSWindow* window = [self window];
  [window setMinSize:NSMakeSize(minWidth, [window minSize].height)];
  if ([window frame].size.width < minWidth) {
    NSRect frame = [window frame];
    frame.size.width = minWidth;
    [window setFrame:frame display:NO];
  }
}

- (void)windowWillClose:(NSNotification*)notification {
  g_exceptionWindow = nil;
  [self autorelease];
}

// Let esc close the window.
- (void)cancel:(id)sender {
  [self closeSheet:self];
}

- (void)keyDown:(NSEvent*)event {
  NSString* chars = [event charactersIgnoringModifiers];
  if ([chars length] == 1) {
    switch ([chars characterAtIndex:0]) {
      case NSDeleteCharacter:
      case NSDeleteFunctionKey:
        // Delete deletes.
        if ([[tableView_ selectedRowIndexes] count] > 0)
          [arrayController_ remove:event];
        return;
    }
  }
  [super keyDown:event];
}

- (void)attachSheetTo:(NSWindow*)window {
  [NSApp beginSheet:[self window]
     modalForWindow:window
      modalDelegate:self
     didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:)
        contextInfo:nil];
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)context {
  [sheet close];
  [sheet orderOut:self];
}

- (IBAction)closeSheet:(id)sender {
  [NSApp endSheet:[self window]];
}


@end

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"

#include "app/l10n_util_mac.h"
#include "base/string_util.h"
#include "grit/generated_resources.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_*
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/extensions/extension_action_context_menu.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/extensions/extension_action.h"

class Extension;

@interface AutocompleteTextFieldEditor(Private)
// Returns the default context menu to be displayed on a right mouse click.
- (NSMenu*)defaultMenuForEvent:(NSEvent*)event;
@end

@implementation AutocompleteTextFieldEditor

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect]))
    dropHandler_.reset([[URLDropTargetHandler alloc] initWithView:self]);
  return self;
}

- (void)copy:(id)sender {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [self performCopy:pb];
}

- (void)cut:(id)sender {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [self performCut:pb];
}

- (void)performCopy:(NSPasteboard*)pb {
  [pb declareTypes:[NSArray array] owner:nil];
  [self writeSelectionToPasteboard:pb types:
      [NSArray arrayWithObject:NSStringPboardType]];
}

- (void)performCut:(NSPasteboard*)pb {
  [self performCopy:pb];
  [self delete:nil];
}

// This class assumes that the delegate is an AutocompleteTextField.
// Enforce that assumption.
- (void)setDelegate:(id)anObject {
  DCHECK(anObject == nil ||
         [anObject isKindOfClass:[AutocompleteTextField class]]);
  [super setDelegate:anObject];
}

// Convenience method for retrieving the observer from the delegate.
- (AutocompleteTextFieldObserver*)observer {
  DCHECK([[self delegate] isKindOfClass:[AutocompleteTextField class]]);
  return [static_cast<AutocompleteTextField*>([self delegate]) observer];
}

- (void)paste:(id)sender {
  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer) {
    observer->OnPaste();
  }
}

- (void)pasteAndGo:sender {
  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer) {
    observer->OnPasteAndGo();
  }
}

// We have rich text, but it shouldn't be modified by the user, so
// don't update the font panel.  In theory, -setUsesFontPanel: should
// accomplish this, but that gets called frequently with YES when
// NSTextField and NSTextView synchronize their contents.  That is
// probably unavoidable because in most cases having rich text in the
// field you probably would expect it to update the font panel.
- (void)updateFontPanel {}

// No ruler bar, so don't update any of that state, either.
- (void)updateRuler {}

- (NSMenu*)menuForEvent:(NSEvent*)event {
  NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];

  // Was the right click within a Page Action? Show a different menu if so.
  NSRect bounds([[self delegate] bounds]);
  AutocompleteTextFieldCell* cell = [[self delegate] autocompleteTextFieldCell];
  const size_t pageActionCount = [cell pageActionCount];
  BOOL flipped = [self isFlipped];
  Browser* browser = BrowserList::GetLastActive();
  // GetLastActive() returns NULL during testing.
  if (!browser)
    return [self defaultMenuForEvent:event];
  ExtensionsService* service = browser->profile()->GetExtensionsService();
  for (size_t i = 0; i < pageActionCount; ++i) {
    NSRect pageActionFrame = [cell pageActionFrameForIndex:i inFrame:bounds];
    if (NSMouseInRect(location, pageActionFrame, flipped)) {
      Extension* extension = service->GetExtensionById(
          [cell pageActionForIndex:i]->extension_id(), false);
      DCHECK(extension);
      if (!extension)
        break;
      return [[[ExtensionActionContextMenu alloc] initWithExtension:extension]
          autorelease];
    }
  }

  // Otherwise, simply return the default menu for this instance.
  return [self defaultMenuForEvent:event];
}

- (NSMenu*)defaultMenuForEvent:(NSEvent*)event {
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@"TITLE"] autorelease];
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CUT)
                  action:@selector(cut:)
           keyEquivalent:@""];
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_COPY)
                  action:@selector(copy:)
           keyEquivalent:@""];
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_PASTE)
                  action:@selector(paste:)
           keyEquivalent:@""];

  // TODO(shess): If the control is not editable, should we show a
  // greyed-out "Paste and Go"?
  if ([self isEditable]) {
    // Paste and go/search.
    AutocompleteTextFieldObserver* observer = [self observer];
    DCHECK(observer);
    if (observer && observer->CanPasteAndGo()) {
      const int string_id = observer->GetPasteActionStringId();
      NSString* label = l10n_util::GetNSStringWithFixup(string_id);

      // TODO(rohitrao): If the clipboard is empty, should we show a
      // greyed-out "Paste and Go" or nothing at all?
      if ([label length]) {
        [menu addItemWithTitle:label
                        action:@selector(pasteAndGo:)
                 keyEquivalent:@""];
      }
    }

    NSString* label = l10n_util::GetNSStringWithFixup(IDS_EDIT_SEARCH_ENGINES);
    DCHECK([label length]);
    if ([label length]) {
      [menu addItem:[NSMenuItem separatorItem]];
      NSMenuItem* item = [menu addItemWithTitle:label
                                         action:@selector(commandDispatch:)
                                  keyEquivalent:@""];
      [item setTag:IDC_EDIT_SEARCH_ENGINES];
    }
  }

  return menu;
}

// (URLDropTarget protocol)
- (id<URLDropTargetController>)urlDropController {
  BrowserWindowController* windowController = [[self window] windowController];
  DCHECK([windowController isKindOfClass:[BrowserWindowController class]]);
  return [windowController toolbarController];
}

// (URLDropTarget protocol)
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  // Make ourself the first responder (even though we're presumably already the
  // first responder), which will select the text to indicate that our contents
  // would be replaced by a drop.
  [[self window] makeFirstResponder:self];
  return [dropHandler_ draggingEntered:sender];
}

// (URLDropTarget protocol)
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  return [dropHandler_ draggingUpdated:sender];
}

// (URLDropTarget protocol)
- (void)draggingExited:(id<NSDraggingInfo>)sender {
  return [dropHandler_ draggingExited:sender];
}

// (URLDropTarget protocol)
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  return [dropHandler_ performDragOperation:sender];
}

@end

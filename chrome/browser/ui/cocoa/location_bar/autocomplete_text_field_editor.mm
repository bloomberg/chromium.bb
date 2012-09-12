// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_editor.h"

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_*
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "grit/generated_resources.h"
#import "ui/base/cocoa/find_pasteboard.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// When too much data is put into a single-line text field, things get
// janky due to the cost of computing the blink rect.  Sometimes users
// accidentally paste large amounts, so place a limit on what will be
// accepted.
//
// 10k characters was arbitrarily chosen by seeing how much a text
// field could handle in a single line before it started getting too
// janky to recover from (jankiness was detectable around 5k).
// www.google.com returns an error for searches around 2k characters,
// so this is conservative.
const NSUInteger kMaxPasteLength = 10000;

// Returns |YES| if too much text would be pasted.
BOOL ThePasteboardIsTooDamnBig() {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSString* type =
      [pb availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]];
  if (!type)
    return NO;

  return [[pb stringForType:type] length] > kMaxPasteLength;
}

}  // namespace

@implementation AutocompleteTextFieldEditor

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    dropHandler_.reset([[URLDropTargetHandler alloc] initWithView:self]);

    forbiddenCharacters_.reset([[NSCharacterSet controlCharacterSet] retain]);
  }
  return self;
}

// If the entire field is selected, drag the same data as would be
// dragged from the field's location icon.  In some cases the textual
// contents will not contain relevant data (for instance, "http://" is
// stripped from URLs).
- (BOOL)dragSelectionWithEvent:(NSEvent *)event
                        offset:(NSSize)mouseOffset
                     slideBack:(BOOL)slideBack {
  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer && observer->CanCopy()) {
    NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    observer->CopyToPasteboard(pboard);

    NSPoint p;
    NSImage* image = [self dragImageForSelectionWithEvent:event origin:&p];

    [self dragImage:image
                 at:p
             offset:mouseOffset
              event:event
         pasteboard:pboard
             source:self
          slideBack:slideBack];
    return YES;
  }
  return [super dragSelectionWithEvent:event
                                offset:mouseOffset
                             slideBack:slideBack];
}

- (void)copy:(id)sender {
  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer && observer->CanCopy())
    observer->CopyToPasteboard([NSPasteboard generalPasteboard]);
}

- (void)cut:(id)sender {
  [self copy:sender];
  [self delete:nil];
}

- (void)copyURL:(id)sender {
  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer->CanCopy())
    observer->CopyURLToPasteboard([NSPasteboard generalPasteboard]);
}

// This class assumes that the delegate is an AutocompleteTextField.
// Enforce that assumption.
- (AutocompleteTextField*)delegate {
  AutocompleteTextField* delegate =
      static_cast<AutocompleteTextField*>([super delegate]);
  DCHECK(delegate == nil ||
         [delegate isKindOfClass:[AutocompleteTextField class]]);
  return delegate;
}

- (void)setDelegate:(AutocompleteTextField*)delegate {
  DCHECK(delegate == nil ||
         [delegate isKindOfClass:[AutocompleteTextField class]]);

  // Unregister from any previously registered undo and redo notifications.
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:self
                name:NSUndoManagerDidUndoChangeNotification
              object:nil];
  [nc removeObserver:self
                name:NSUndoManagerDidRedoChangeNotification
              object:nil];

  // Set the delegate.
  [super setDelegate:delegate];

  // Register for undo and redo notifications from the new |delegate|, if it is
  // non-nil.
  if ([self delegate]) {
    NSUndoManager* undo_manager = [self undoManager];
    [nc addObserver:self
           selector:@selector(didUndoOrRedo:)
               name:NSUndoManagerDidUndoChangeNotification
             object:undo_manager];
    [nc addObserver:self
           selector:@selector(didUndoOrRedo:)
               name:NSUndoManagerDidRedoChangeNotification
             object:undo_manager];
  }
}

- (void)didUndoOrRedo:(NSNotification *)aNotification {
  AutocompleteTextFieldObserver* observer = [self observer];
  if (observer)
    observer->OnDidChange();
}

// Convenience method for retrieving the observer from the delegate.
- (AutocompleteTextFieldObserver*)observer {
  return [[self delegate] observer];
}

- (void)paste:(id)sender {
  if (ThePasteboardIsTooDamnBig()) {
    NSBeep();
    return;
  }

  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer) {
    observer->OnPaste();
  }
}

- (void)pasteAndMatchStyle:(id)sender {
  [self paste:sender];
}

- (void)pasteAndGo:sender {
  if (ThePasteboardIsTooDamnBig()) {
    NSBeep();
    return;
  }

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
  // Give the control a chance to provide page-action menus.
  // NOTE: Note that page actions aren't even in the editor's
  // boundaries!  The Cocoa control implementation seems to do a
  // blanket forward to here if nothing more specific is returned from
  // the control and cell calls.
  // TODO(shess): Determine if the page-action part of this can be
  // moved to the cell.
  NSMenu* actionMenu = [[self delegate] decorationMenuForEvent:event];
  if (actionMenu)
    return actionMenu;

  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@"TITLE"] autorelease];
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CUT)
                  action:@selector(cut:)
           keyEquivalent:@""];
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_COPY)
                  action:@selector(copy:)
           keyEquivalent:@""];

  if ([self isEditable]) {
    // Copy URL if the URL has been replaced by the Extended Instant API.
    DCHECK([self observer]);
    NSString* label = l10n_util::GetNSStringWithFixup(IDS_COPY_URL_MAC);
    [menu addItemWithTitle:label
                    action:@selector(copyURL:)
             keyEquivalent:@""];
  }

  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_PASTE)
                  action:@selector(paste:)
           keyEquivalent:@""];

  // TODO(shess): If the control is not editable, should we show a
  // greyed-out "Paste and Go"?
  if ([self isEditable]) {
    // Paste and go/search.
    AutocompleteTextFieldObserver* observer = [self observer];
    DCHECK(observer);
    const int string_id = observer->GetPasteActionStringId();
    NSString* label = l10n_util::GetNSStringWithFixup(string_id);
    DCHECK([label length]);
    [menu addItemWithTitle:label
                    action:@selector(pasteAndGo:)
             keyEquivalent:@""];

    NSString* search_engine_label =
        l10n_util::GetNSStringWithFixup(IDS_EDIT_SEARCH_ENGINES);
    DCHECK([search_engine_label length]);
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* item = [menu addItemWithTitle:search_engine_label
                                       action:@selector(commandDispatch:)
                                keyEquivalent:@""];
    [item setTag:IDC_EDIT_SEARCH_ENGINES];
  }

  return menu;
}

// (Overridden from NSResponder)
- (BOOL)becomeFirstResponder {
  BOOL doAccept = [super becomeFirstResponder];
  AutocompleteTextField* field = [self delegate];
  // Only lock visibility if we've been set up with a delegate (the text field).
  if (doAccept && field) {
    // Give the text field ownership of the visibility lock. (The first
    // responder dance between the field and the field editor is a little
    // weird.)
    [[BrowserWindowController browserWindowControllerForView:field]
        lockBarVisibilityForOwner:field withAnimation:YES delay:NO];
  }
  return doAccept;
}

// (Overridden from NSResponder)
- (BOOL)resignFirstResponder {
  BOOL doResign = [super resignFirstResponder];
  AutocompleteTextField* field = [self delegate];
  // Only lock visibility if we've been set up with a delegate (the text field).
  if (doResign && field) {
    // Give the text field ownership of the visibility lock.
    [[BrowserWindowController browserWindowControllerForView:field]
        releaseBarVisibilityForOwner:field withAnimation:YES delay:YES];

    AutocompleteTextFieldObserver* observer = [self observer];
    if (observer)
      observer->OnKillFocus();
  }
  return doResign;
}

// (URLDropTarget protocol)
- (id<URLDropTargetController>)urlDropController {
  BrowserWindowController* windowController =
      [BrowserWindowController browserWindowControllerForView:self];
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

// Prevent control characters from being entered into the Omnibox.
// This is invoked for keyboard entry, not for pasting.
- (void)insertText:(id)aString {
  // Repeatedly remove control characters.  The loop will only ever
  // execute at all when the user enters control characters (using
  // Ctrl-Alt- or Ctrl-Q).  Making this generally efficient would
  // probably be a loss, since the input always seems to be a single
  // character.
  if ([aString isKindOfClass:[NSAttributedString class]]) {
    NSRange range =
        [[aString string] rangeOfCharacterFromSet:forbiddenCharacters_];
    while (range.location != NSNotFound) {
      aString = [[aString mutableCopy] autorelease];
      [aString deleteCharactersInRange:range];
      range = [[aString string] rangeOfCharacterFromSet:forbiddenCharacters_];
    }
    DCHECK_EQ(range.length, 0U);
  } else {
    NSRange range = [aString rangeOfCharacterFromSet:forbiddenCharacters_];
    while (range.location != NSNotFound) {
      aString =
          [aString stringByReplacingCharactersInRange:range withString:@""];
      range = [aString rangeOfCharacterFromSet:forbiddenCharacters_];
    }
    DCHECK_EQ(range.length, 0U);
  }

  // NOTE: If |aString| is empty, this intentionally replaces the
  // selection with empty.  This seems consistent with the case where
  // the input contained a mixture of characters and the string ended
  // up not empty.
  [super insertText:aString];
}

- (void)setMarkedText:(id)aString selectedRange:(NSRange)selRange {
  if (![self hasMarkedText]) {
    // Before input methods set composition text in the omnibox, we need to
    // examine whether the autocompletion controller accepts the keyword to
    // avoid committing the current composition text wrongly.
    AutocompleteTextFieldObserver* observer = [self observer];
    if (observer)
      observer->OnStartingIME();
  }

  [super setMarkedText:aString selectedRange:selRange];

  // Because the OmniboxViewMac class treats marked text as content,
  // we need to treat the change to marked text as content change as well.
  [self didChangeText];
}

- (NSRange)selectionRangeForProposedRange:(NSRange)proposedSelRange
                              granularity:(NSSelectionGranularity)granularity {
  AutocompleteTextFieldObserver* observer = [self observer];
  NSRange modifiedRange = [super selectionRangeForProposedRange:proposedSelRange
                                                    granularity:granularity];
  if (observer)
    return observer->SelectionRangeForProposedRange(modifiedRange);
  return modifiedRange;
}




- (void)setSelectedRange:(NSRange)charRange
                affinity:(NSSelectionAffinity)affinity
          stillSelecting:(BOOL)flag {
  [super setSelectedRange:charRange affinity:affinity stillSelecting:flag];

  // We're only interested in selection changes directly caused by keyboard
  // input from the user.
  if (interpretingKeyEvents_)
    textChangedByKeyEvents_ = YES;
}

- (void)interpretKeyEvents:(NSArray *)eventArray {
  DCHECK(!interpretingKeyEvents_);
  interpretingKeyEvents_ = YES;
  textChangedByKeyEvents_ = NO;
  AutocompleteTextFieldObserver* observer = [self observer];

  if (observer)
    observer->OnBeforeChange();

  [super interpretKeyEvents:eventArray];

  if (textChangedByKeyEvents_ && observer)
    observer->OnDidChange();

  DCHECK(interpretingKeyEvents_);
  interpretingKeyEvents_ = NO;
}

- (BOOL)shouldChangeTextInRange:(NSRange)affectedCharRange
              replacementString:(NSString *)replacementString {
  BOOL ret = [super shouldChangeTextInRange:affectedCharRange
                          replacementString:replacementString];

  if (ret && !interpretingKeyEvents_) {
    AutocompleteTextFieldObserver* observer = [self observer];
    if (observer)
      observer->OnBeforeChange();
  }
  return ret;
}

- (void)didChangeText {
  [super didChangeText];

  AutocompleteTextFieldObserver* observer = [self observer];
  if (observer) {
    if (!interpretingKeyEvents_ &&
        ![[self undoManager] isUndoing] && ![[self undoManager] isRedoing]) {
      observer->OnDidChange();
    } else if (interpretingKeyEvents_) {
      textChangedByKeyEvents_ = YES;
    }
  }
}

- (void)doCommandBySelector:(SEL)cmd {
  // TODO(shess): Review code for cases where we're fruitlessly attempting to
  // work in spite of not having an observer.
  AutocompleteTextFieldObserver* observer = [self observer];

  if (observer && observer->OnDoCommandBySelector(cmd)) {
    // The observer should already be aware of any changes to the text, so
    // setting |textChangedByKeyEvents_| to NO to prevent its OnDidChange()
    // method from being called unnecessarily.
    textChangedByKeyEvents_ = NO;
    return;
  }

  // If the escape key was pressed and no revert happened and we're in
  // fullscreen mode, give focus to the web contents, which may dismiss the
  // overlay.
  if (cmd == @selector(cancelOperation:)) {
    BrowserWindowController* windowController =
        [BrowserWindowController browserWindowControllerForView:self];
    if ([windowController isFullscreen]) {
      [windowController focusTabContents];
      textChangedByKeyEvents_ = NO;
      return;
    }
  }

  [super doCommandBySelector:cmd];
}

- (void)setAttributedString:(NSAttributedString*)aString {
  NSTextStorage* textStorage = [self textStorage];
  DCHECK(textStorage);
  [textStorage setAttributedString:aString];

  // The text has been changed programmatically. The observer should know
  // this change, so setting |textChangedByKeyEvents_| to NO to
  // prevent its OnDidChange() method from being called unnecessarily.
  textChangedByKeyEvents_ = NO;
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
  if ([item action] == @selector(copyToFindPboard:))
    return [self selectedRange].length > 0;
  if ([item action] == @selector(pasteAndGo:)) {
    // TODO(rohitrao): If the clipboard is empty, should we show a
    // greyed-out "Paste and Go" or nothing at all?
    AutocompleteTextFieldObserver* observer = [self observer];
    DCHECK(observer);
    return observer->CanPasteAndGo();
  }
  if ([item action] == @selector(copyURL:)) {
    AutocompleteTextFieldObserver* observer = [self observer];
    DCHECK(observer);
    return observer->ShouldEnableCopyURL();
  }
  return [super validateMenuItem:item];
}

- (void)copyToFindPboard:(id)sender {
  NSRange selectedRange = [self selectedRange];
  if (selectedRange.length == 0)
    return;
  NSAttributedString* selection =
      [self attributedSubstringForProposedRange:selectedRange
                                    actualRange:NULL];
  if (!selection)
    return;

  [[FindPasteboard sharedInstance] setFindText:[selection string]];
}

@end

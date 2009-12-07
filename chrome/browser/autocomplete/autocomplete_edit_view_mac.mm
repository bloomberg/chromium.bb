// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"

#include <Carbon/Carbon.h>  // kVK_Return

#include "app/clipboard/clipboard.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cocoa/event_utils.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/toolbar_model.h"
#include "grit/generated_resources.h"

// Focus-handling between |field_| and |model_| is a bit subtle.
// Other platforms detect change of focus, which is inconvenient
// without subclassing NSTextField (even with a subclass, the use of a
// field editor may complicate things).
//
// |model_| doesn't actually do anything when it gains focus, it just
// initializes.  Visible activity happens only after the user edits.
// NSTextField delegate receives messages around starting and ending
// edits, so that sufcices to catch focus changes.  Since all calls
// into |model_| start from AutocompleteEditViewMac, in the worst case
// we can add code to sync up the sense of focus as needed.
//
// I've added DCHECK(IsFirstResponder()) in the places which I believe
// should only be reachable when |field_| is being edited.  If these
// fire, it probably means someone unexpected is calling into
// |model_|.
//
// Other platforms don't appear to have the sense of "key window" that
// Mac does (I believe their fields lose focus when the window loses
// focus).  Rather than modifying focus outside the control's edit
// scope, when the window resigns key the autocomplete popup is
// closed.  |model_| still believes it has focus, and the popup will
// be regenerated on the user's next edit.  That seems to match how
// things work on other platforms.

namespace {

// TODO(shess): This is ugly, find a better way.  Using it right now
// so that I can crib from gtk and still be able to see that I'm using
// the same values easily.
const NSColor* ColorWithRGBBytes(int rr, int gg, int bb) {
  DCHECK_LE(rr, 255);
  DCHECK_LE(bb, 255);
  DCHECK_LE(gg, 255);
  return [NSColor colorWithCalibratedRed:static_cast<float>(rr)/255.0
                                   green:static_cast<float>(gg)/255.0
                                    blue:static_cast<float>(bb)/255.0
                                   alpha:1.0];
}
const NSColor* SecureBackgroundColor() {
  return ColorWithRGBBytes(255, 245, 195);  // Yellow
}
const NSColor* NormalBackgroundColor() {
  return [NSColor controlBackgroundColor];
}
const NSColor* InsecureBackgroundColor() {
  return [NSColor controlBackgroundColor];
}

const NSColor* HostTextColor() {
  return [NSColor blackColor];
}
const NSColor* BaseTextColor() {
  return [NSColor darkGrayColor];
}
const NSColor* SecureSchemeColor() {
  return ColorWithRGBBytes(0x00, 0x96, 0x14);
}
const NSColor* InsecureSchemeColor() {
  return ColorWithRGBBytes(0xc8, 0x00, 0x00);
}

// Store's the model and view state across tab switches.
struct AutocompleteEditViewMacState {
  AutocompleteEditViewMacState(const AutocompleteEditModel::State model_state,
                               const bool has_focus, const NSRange& selection)
      : model_state(model_state),
        has_focus(has_focus),
        selection(selection) {
  }

  const AutocompleteEditModel::State model_state;
  const bool has_focus;
  const NSRange selection;
};

// Returns a lazily initialized property bag accessor for saving our
// state in a TabContents.  When constructed |accessor| generates a
// globally-unique id used to index into the per-tab PropertyBag used
// to store the state data.
PropertyAccessor<AutocompleteEditViewMacState>* GetStateAccessor() {
  static PropertyAccessor<AutocompleteEditViewMacState> accessor;
  return &accessor;
}

// Accessors for storing and getting the state from the tab.
void StoreStateToTab(TabContents* tab,
                     const AutocompleteEditViewMacState& state) {
  GetStateAccessor()->SetProperty(tab->property_bag(), state);
}
const AutocompleteEditViewMacState* GetStateFromTab(const TabContents* tab) {
  return GetStateAccessor()->GetProperty(tab->property_bag());
}

// Helper to make converting url_parse ranges to NSRange easier to
// read.
NSRange ComponentToNSRange(const url_parse::Component& component) {
  return NSMakeRange(static_cast<NSInteger>(component.begin),
                     static_cast<NSInteger>(component.len));
}

}  // namespace

// TODO(shess): AutocompletePopupViewMac doesn't really need an
// NSTextField.  It wants to know where the position the popup, what
// font to use, and it also needs to be able to attach the popup to
// the window |field_| is in.
AutocompleteEditViewMac::AutocompleteEditViewMac(
    AutocompleteEditController* controller,
    const BubblePositioner* bubble_positioner,
    ToolbarModel* toolbar_model,
    Profile* profile,
    CommandUpdater* command_updater,
    AutocompleteTextField* field)
    : model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(new AutocompletePopupViewMac(
          this, model_.get(), bubble_positioner, profile, field)),
      controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      field_(field) {
  DCHECK(controller);
  DCHECK(toolbar_model);
  DCHECK(profile);
  DCHECK(command_updater);
  DCHECK(field);
  [field_ setObserver:this];

  // Needed so that editing doesn't lose the styling.
  [field_ setAllowsEditingTextAttributes:YES];
}

AutocompleteEditViewMac::~AutocompleteEditViewMac() {
  // TODO(shess): Having to be aware of destructor ordering in this
  // way seems brittle.  There must be a better way.

  // Destroy popup view before this object in case it tries to call us
  // back in the destructor.  Likewise for destroying the model before
  // this object.
  popup_view_.reset();
  model_.reset();

  // Disconnect from |field_|, it outlives this object.
  [field_ setObserver:NULL];
}

void AutocompleteEditViewMac::SaveStateToTab(TabContents* tab) {
  DCHECK(tab);

  const bool hasFocus = [field_ currentEditor] ? true : false;

  NSRange range;
  if (hasFocus) {
    range = GetSelectedRange();
  } else {
    // If we are not focussed, there is no selection.  Manufacture
    // something reasonable in case it starts to matter in the future.
    range = NSMakeRange(0, [[field_ stringValue] length]);
  }

  AutocompleteEditViewMacState state(model_->GetStateForTabSwitch(),
                                     hasFocus, range);
  StoreStateToTab(tab, state);
}

void AutocompleteEditViewMac::Update(
    const TabContents* tab_for_state_restoring) {
  // TODO(shess): It seems like if the tab is non-NULL, then this code
  // shouldn't need to be called at all.  When coded that way, I find
  // that the field isn't always updated correctly.  Figure out why
  // this is.  Maybe this method should be refactored into more
  // specific cases.
  const bool user_visible =
      model_->UpdatePermanentText(toolbar_model_->GetText());

  if (tab_for_state_restoring) {
    RevertAll();

    const AutocompleteEditViewMacState* state =
        GetStateFromTab(tab_for_state_restoring);
    if (state) {
      // Should restore the user's text via SetUserText().
      model_->RestoreState(state->model_state);

      // Restore focus and selection if they were present when the tab
      // was switched away.
      if (state->has_focus) {
        // TODO(shess): Unfortunately, there is no safe way to update
        // this because TabStripController -selectTabWithContents:* is
        // also messing with focus.  Both parties need to agree to
        // store existing state before anyone tries to setup the new
        // state.  Anyhow, it would look something like this.
#if 0
        [[field_ window] makeFirstResponder:field_];
        [[field_ currentEditor] setSelectedRange:state->selection];
#endif
      }
    }
  } else if (user_visible) {
    // Restore everything to the baseline look.
    RevertAll();
    // TODO(shess): Figure out how this case is used, to make sure
    // we're getting the selection and popup right.

  } else {
    // TODO(shess): This corresponds to _win and _gtk, except those
    // guard it with a test for whether the security level changed.
    // But AFAICT, that can only change if the text changed, and that
    // code compares the toolbar_model_ security level with the local
    // security level.  Dig in and figure out why this isn't a no-op
    // that should go away.
    EmphasizeURLComponents();
  }
}

void AutocompleteEditViewMac::OpenURL(const GURL& url,
                                      WindowOpenDisposition disposition,
                                      PageTransition::Type transition,
                                      const GURL& alternate_nav_url,
                                      size_t selected_line,
                                      const std::wstring& keyword) {
  // TODO(shess): Why is the caller passing an invalid url in the
  // first place?  Make sure that case isn't being dropped on the
  // floor.
  if (!url.is_valid()) {
    return;
  }

  model_->SendOpenNotification(selected_line, keyword);

  if (disposition != NEW_BACKGROUND_TAB)
    RevertAll();  // Revert the box to its unedited state.
  controller_->OnAutocompleteAccept(url, disposition, transition,
                                    alternate_nav_url);
}

std::wstring AutocompleteEditViewMac::GetText() const {
  return base::SysNSStringToWide([field_ stringValue]);
}

void AutocompleteEditViewMac::SetUserText(const std::wstring& text,
                                          const std::wstring& display_text,
                                          bool update_popup) {
  model_->SetUserText(text);
  // TODO(shess): TODO below from gtk.
  // TODO(deanm): something about selection / focus change here.
  SetText(display_text);
  if (update_popup) {
    UpdatePopup();
  }
  controller_->OnChanged();
}

NSRange AutocompleteEditViewMac::GetSelectedRange() const {
  DCHECK([field_ currentEditor]);
  return [[field_ currentEditor] selectedRange];
}

void AutocompleteEditViewMac::SetSelectedRange(const NSRange range) {
  // This can be called when we don't have focus.  For instance, when
  // the user clicks the "Go" button.
  if (model_->has_focus()) {
    // TODO(shess): If |model_| thinks we have focus, this should not
    // be necessary.  Try to convert to DCHECK(IsFirstResponder()).
    if (![field_ currentEditor]) {
      [[field_ window] makeFirstResponder:field_];
    }

    // TODO(shess): What if it didn't get first responder, and there is
    // no field editor?  This will do nothing.  Well, at least it won't
    // crash.  Think of something more productive to do, or prove that
    // it cannot occur and DCHECK appropriately.
    [[field_ currentEditor] setSelectedRange:range];
  }
}

void AutocompleteEditViewMac::SetWindowTextAndCaretPos(const std::wstring& text,
                                                       size_t caret_pos) {
  DCHECK_LE(caret_pos, text.size());
  SetTextAndSelectedRange(text, NSMakeRange(caret_pos, caret_pos));
}

void AutocompleteEditViewMac::SetForcedQuery() {
  // We need to do this first, else |SetSelectedRange()| won't work.
  FocusLocation();

  const std::wstring current_text(GetText());
  if (current_text.empty() || (current_text[0] != '?')) {
    SetUserText(L"?");
  } else {
    NSRange range = NSMakeRange(1, current_text.size() - 1);
    [[field_ currentEditor] setSelectedRange:range];
  }
}

bool AutocompleteEditViewMac::IsSelectAll() {
  if (![field_ currentEditor])
    return true;
  const NSRange all_range = NSMakeRange(0, [[field_ stringValue] length]);
  return NSEqualRanges(all_range, GetSelectedRange());
}

void AutocompleteEditViewMac::SelectAll(bool reversed) {
  // TODO(shess): Figure out what |reversed| implies.  The gtk version
  // has it imply inverting the selection front to back, but I don't
  // even know if that makes sense for Mac.

  // TODO(shess): Verify that we should be stealing focus at this
  // point.
  SetSelectedRange(NSMakeRange(0, GetText().size()));
}

void AutocompleteEditViewMac::RevertAll() {
  ClosePopup();
  model_->Revert();

  // TODO(shess): This should be a no-op, the results from GetText()
  // could only get there via UpdateAndStyleText() in the first place.
  // Dig into where this code can be called from and see if this line
  // can be removed.
  EmphasizeURLComponents();
  controller_->OnChanged();
  [field_ clearUndoChain];
}

void AutocompleteEditViewMac::UpdatePopup() {
  model_->SetInputInProgress(true);
  if (!model_->has_focus())
    return;

  // TODO(shess):
  // Shouldn't inline autocomplete when the caret/selection isn't at
  // the end of the text.
  //
  // One option would seem to be to check for a non-nil field
  // editor, and check it's selected range against its length.
  model_->StartAutocomplete(false);
}

void AutocompleteEditViewMac::ClosePopup() {
  popup_view_->GetModel()->StopAutocomplete();
}

void AutocompleteEditViewMac::SetFocus() {
}

void AutocompleteEditViewMac::SetText(const std::wstring& display_text) {
  NSString* ss = base::SysWideToNSString(display_text);
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:ss] autorelease];
  NSFont* font = ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).nativeFont();
  [as addAttribute:NSFontAttributeName value:font
             range:NSMakeRange(0, [as length])];

  url_parse::Parsed parts;
  AutocompleteInput::Parse(display_text, model_->GetDesiredTLD(),
                           &parts, NULL);
  const bool emphasize = model_->CurrentTextIsURL() && (parts.host.len > 0);
  if (emphasize) {
    [as addAttribute:NSForegroundColorAttributeName value:BaseTextColor()
               range:NSMakeRange(0, [as length])];

    [as addAttribute:NSForegroundColorAttributeName value:HostTextColor()
               range:ComponentToNSRange(parts.host)];
  }

  // TODO(shess): GTK has this as a member var, figure out why.
  // [Could it be to not change if no change?  If so, I'm guessing
  // AppKit may already handle that.]
  const ToolbarModel::SecurityLevel scheme_security_level =
      toolbar_model_->GetSchemeSecurityLevel();

  if (scheme_security_level == ToolbarModel::SECURE) {
    [field_ setBackgroundColor:SecureBackgroundColor()];
  } else if (scheme_security_level == ToolbarModel::NORMAL) {
    [field_ setBackgroundColor:NormalBackgroundColor()];
  } else if (scheme_security_level == ToolbarModel::INSECURE) {
    [field_ setBackgroundColor:InsecureBackgroundColor()];
  } else {
    NOTREACHED() << "Unexpected scheme_security_level: "
                 << scheme_security_level;
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model_->user_input_in_progress() && parts.scheme.is_nonempty() &&
      (scheme_security_level != ToolbarModel::NORMAL)) {
    NSColor* color;
    if (scheme_security_level == ToolbarModel::SECURE) {
      color = SecureSchemeColor();
    } else {
      color = InsecureSchemeColor();
      // Add a strikethrough through the scheme.
      [as addAttribute:NSStrikethroughStyleAttributeName
                 value:[NSNumber numberWithInt:NSUnderlineStyleSingle]
                 range:ComponentToNSRange(parts.scheme)];
    }
    [as addAttribute:NSForegroundColorAttributeName value:color
               range:ComponentToNSRange(parts.scheme)];
  }

  [field_ setAttributedStringValue:as];

  // TODO(shess): This may be an appropriate place to call:
  //   controller_->OnChanged();
  // In the current implementation, this tells LocationBarViewMac to
  // mess around with |model_| and update |field_|.  Unfortunately,
  // when I look at our peer implementations, it's not entirely clear
  // to me if this is safe.  SetText() is sort of an utility method,
  // and different callers sometimes have different needs.  Research
  // this issue so that it can be added safely.

  // TODO(shess): Also, consider whether this code couldn't just
  // manage things directly.  Windows uses a series of overlaid view
  // objects to accomplish the hinting stuff that OnChanged() does, so
  // it makes sense to have it in the controller that lays those
  // things out.  Mac instead pushes the support into a custom
  // text-field implementation.
}

void AutocompleteEditViewMac::SetTextAndSelectedRange(
    const std::wstring& display_text, const NSRange range) {
  SetText(display_text);
  SetSelectedRange(range);
}

void AutocompleteEditViewMac::EmphasizeURLComponents() {
  if ([field_ currentEditor]) {
    SetTextAndSelectedRange(GetText(), GetSelectedRange());
  } else {
    SetText(GetText());
  }
}

void AutocompleteEditViewMac::OnTemporaryTextMaybeChanged(
    const std::wstring& display_text, bool save_original_selection) {
  // TODO(shess): I believe this is for when the user arrows around
  // the popup, will be restored if they hit escape.  Figure out if
  // that is for certain it.
  if (save_original_selection)
    saved_temporary_selection_ = GetSelectedRange();

  SetWindowTextAndCaretPos(display_text, display_text.size());
  controller_->OnChanged();
  [field_ clearUndoChain];
}

bool AutocompleteEditViewMac::OnInlineAutocompleteTextMaybeChanged(
    const std::wstring& display_text, size_t user_text_length) {
  // TODO(shess): Make sure that this actually works.  The round trip
  // to native form and back may mean that it's the same but not the
  // same.
  if (display_text == GetText()) {
    return false;
  }

  DCHECK_LE(user_text_length, display_text.size());
  const NSRange range = NSMakeRange(user_text_length, display_text.size());
  SetTextAndSelectedRange(display_text, range);
  controller_->OnChanged();
  [field_ clearUndoChain];

  return true;
}

void AutocompleteEditViewMac::OnRevertTemporaryText() {
  SetSelectedRange(saved_temporary_selection_);
}

bool AutocompleteEditViewMac::IsFirstResponder() const {
  return [field_ currentEditor] != nil ? true : false;
}

void AutocompleteEditViewMac::OnBeforePossibleChange() {
  // We should only arrive here when the field is focussed.
  DCHECK(IsFirstResponder());

  selection_before_change_ = GetSelectedRange();
  text_before_change_ = GetText();
}

bool AutocompleteEditViewMac::OnAfterPossibleChange() {
  // We should only arrive here when the field is focussed.
  DCHECK(IsFirstResponder());

  const NSRange new_selection(GetSelectedRange());
  const std::wstring new_text(GetText());
  const size_t length = new_text.length();

  const bool selection_differs = !NSEqualRanges(new_selection,
                                                selection_before_change_);
  const bool at_end_of_edit = (length == new_selection.location);
  const bool text_differs = (new_text != text_before_change_);

  // When the user has deleted text, we don't allow inline
  // autocomplete.  This is assumed if the text has gotten shorter AND
  // the selection has shifted towards the front of the text.  During
  // normal typing the text will almost always be shorter (as the new
  // input replaces the autocomplete suggestion), but in that case the
  // selection point will have moved towards the end of the text.
  // TODO(shess): In our implementation, we can catch -deleteBackward:
  // and other methods to provide positive knowledge that a delete
  // occured, rather than intuiting it from context.  Consider whether
  // that would be a stronger approach.
  const bool just_deleted_text =
      (length < text_before_change_.length() &&
       new_selection.location <= selection_before_change_.location);

  const bool something_changed = model_->OnAfterPossibleChange(new_text,
      selection_differs, text_differs, just_deleted_text, at_end_of_edit);

  // Restyle in case the user changed something.
  // TODO(shess): I believe there are multiple-redraw cases, here.
  // Linux watches for something_changed && text_differs, but that
  // fails for us in case you copy the URL and paste the identical URL
  // back (we'll lose the styling).
  EmphasizeURLComponents();
  controller_->OnChanged();

  return something_changed;
}

gfx::NativeView AutocompleteEditViewMac::GetNativeView() const {
  return field_;
}

void AutocompleteEditViewMac::OnDidBeginEditing() {
  // We should only arrive here when the field is focussed.
  DCHECK([field_ currentEditor]);

  NSEvent* theEvent = [NSApp currentEvent];
  const bool controlDown = ([theEvent modifierFlags]&NSControlKeyMask) != 0;
  model_->OnSetFocus(controlDown);

  // Capture the current state.
  OnBeforePossibleChange();
}

void AutocompleteEditViewMac::OnDidChange() {
  // Figure out what changed and notify the model_.
  OnAfterPossibleChange();

  // Then capture the new state.
  OnBeforePossibleChange();
}

void AutocompleteEditViewMac::OnDidEndEditing() {
  ClosePopup();

  // Tell the model to reset itself.
  model_->OnKillFocus();
}

bool AutocompleteEditViewMac::OnDoCommandBySelector(SEL cmd) {
  // We should only arrive here when the field is focussed.
  DCHECK(IsFirstResponder());

  // Don't intercept up/down-arrow if the popup isn't open.
  if (popup_view_->IsOpen()) {
    if (cmd == @selector(moveDown:)) {
      model_->OnUpOrDownKeyPressed(1);
      return true;
    }

    if (cmd == @selector(moveUp:)) {
      model_->OnUpOrDownKeyPressed(-1);
      return true;
    }
  }

  if (cmd == @selector(scrollPageDown:)) {
    model_->OnUpOrDownKeyPressed(model_->result().size());
    return true;
  }

  if (cmd == @selector(scrollPageUp:)) {
    model_->OnUpOrDownKeyPressed(-model_->result().size());
    return true;
  }

  if (cmd == @selector(cancelOperation:)) {
    model_->OnEscapeKeyPressed();
    return true;
  }

  if (cmd == @selector(insertTab:)) {
    if (model_->is_keyword_hint() && !model_->keyword().empty()) {
      model_->AcceptKeyword();
      return true;
    }
  }

  // TODO(shess): Option-tab, would normally insert a literal tab
  // character.  Consider combining with -insertTab:
  if (cmd == @selector(insertTabIgnoringFieldEditor:)) {
    return true;
  }

  // |-noop:| is sent when the user presses Cmd+Return. Override the no-op
  // behavior with the proper WindowOpenDisposition.
  NSEvent* event = [NSApp currentEvent];
  if (cmd == @selector(insertNewline:) ||
     (cmd == @selector(noop:) && [event keyCode] == kVK_Return)) {
    WindowOpenDisposition disposition =
        event_utils::WindowOpenDispositionFromNSEvent(event);
    model_->AcceptInput(disposition, false);
    // Opening a URL in a background tab should also revert the omnibox contents
    // to their original state.  We cannot do a blanket revert in OpenURL()
    // because middle-clicks also open in a new background tab, but those should
    // not revert the omnibox text.
    RevertAll();
    return true;
  }

  // Option-Return
  if (cmd == @selector(insertNewlineIgnoringFieldEditor:)) {
    model_->AcceptInput(NEW_FOREGROUND_TAB, false);
    return true;
  }

  // When the user does Control-Enter, the existing content has "www."
  // prepended and ".com" appended.  |model_| should already have
  // received notification when the Control key was depressed, but it
  // is safe to tell it twice.
  if (cmd == @selector(insertLineBreak:)) {
    OnControlKeyChanged(true);
    WindowOpenDisposition disposition =
        event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
    model_->AcceptInput(disposition, false);
    return true;
  }

  if (cmd == @selector(deleteBackward:)) {
    if (OnBackspacePressed()) {
      return true;
    }
  }

  if (cmd == @selector(deleteForward:)) {
    const NSUInteger modifiers = [[NSApp currentEvent] modifierFlags];
    if ((modifiers & NSShiftKeyMask) != 0) {
      if (popup_view_->IsOpen()) {
        popup_view_->GetModel()->TryDeletingCurrentItem();
        return true;
      }
    }
  }

  // Capture the state before the operation changes the content.
  // TODO(shess): Determine if this is always redundent WRT the call
  // in -controlTextDidChange:.
  OnBeforePossibleChange();
  return false;
}

void AutocompleteEditViewMac::OnDidResignKey() {
  ClosePopup();
}

void AutocompleteEditViewMac::OnPaste() {
  // This code currently expects |field_| to be focussed.
  DCHECK([field_ currentEditor]);

  std::wstring text = GetClipboardText(g_browser_process->clipboard());
  if (text.empty()) {
    return;
  }
  NSString* s = base::SysWideToNSString(text);

  // -shouldChangeTextInRange:* and -didChangeText are documented in
  // NSTextView as things you need to do if you write additional
  // user-initiated editing functions.  They cause the appropriate
  // delegate methods to be called.
  // TODO(shess): It would be nice to separate the Cocoa-specific code
  // from the Chrome-specific code.
  NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
  const NSRange selectedRange = GetSelectedRange();
  if ([editor shouldChangeTextInRange:selectedRange replacementString:s]) {
    // If this paste will be replacing all the text, record that, so
    // we can do different behaviors in such a case.
    if (IsSelectAll())
      model_->on_paste_replacing_all();

    // Force a Paste operation to trigger the text_changed code in
    // OnAfterPossibleChange(), even if identical contents are pasted
    // into the text box.
    text_before_change_.clear();

    [editor replaceCharactersInRange:selectedRange withString:s];
    [editor didChangeText];
  }
}

bool AutocompleteEditViewMac::CanPasteAndGo() {
  return
    model_->CanPasteAndGo(GetClipboardText(g_browser_process->clipboard()));
}

int AutocompleteEditViewMac::GetPasteActionStringId() {
  DCHECK(CanPasteAndGo());

  // Use PASTE_AND_SEARCH as the default fallback (although the DCHECK above
  // should never trigger).
  if (!model_->is_paste_and_search())
    return IDS_PASTE_AND_GO;
  else
    return IDS_PASTE_AND_SEARCH;
}

void AutocompleteEditViewMac::OnPasteAndGo() {
  if (CanPasteAndGo())
    model_->PasteAndGo();
}

void AutocompleteEditViewMac::OnFrameChanged() {
  // TODO(shess): UpdatePopupAppearance() is called frequently, so it
  // should be really cheap, but in this case we could probably make
  // things even cheaper by refactoring between the popup-placement
  // code and the matrix-population code.
  popup_view_->UpdatePopupAppearance();

  // Give controller a chance to rearrange decorations.
  controller_->OnChanged();
}

bool AutocompleteEditViewMac::OnBackspacePressed() {
  // Don't intercept if not in keyword search mode.
  if (model_->is_keyword_hint() || model_->keyword().empty()) {
    return false;
  }

  // Don't intercept if there is a selection, or the cursor isn't at
  // the leftmost position.
  const NSRange selection = GetSelectedRange();
  if (selection.length > 0 || selection.location > 0) {
    return false;
  }

  // We're showing a keyword and the user pressed backspace at the
  // beginning of the text.  Delete the selected keyword.
  model_->ClearKeyword(GetText());
  return true;
}

void AutocompleteEditViewMac::OnControlKeyChanged(bool pressed) {
  model_->OnControlKeyChanged(pressed);
}

void AutocompleteEditViewMac::FocusLocation() {
  [[field_ window] makeFirstResponder:field_];
  DCHECK_EQ([field_ currentEditor], [[field_ window] firstResponder]);
}

// TODO(shess): Copied from autocomplete_edit_view_win.cc.  Could this
// be pushed into the model?
std::wstring AutocompleteEditViewMac::GetClipboardText(Clipboard* clipboard) {
  // autocomplete_edit_view_win.cc assumes this can never happen, we
  // will too.
  DCHECK(clipboard);

  if (clipboard->IsFormatAvailable(Clipboard::GetPlainTextWFormatType(),
                                   Clipboard::BUFFER_STANDARD)) {
    string16 text16;
    clipboard->ReadText(Clipboard::BUFFER_STANDARD, &text16);

    // Note: Unlike in the find popup and textfield view, here we completely
    // remove whitespace strings containing newlines.  We assume users are
    // most likely pasting in URLs that may have been split into multiple
    // lines in terminals, email programs, etc., and so linebreaks indicate
    // completely bogus whitespace that would just cause the input to be
    // invalid.
    return CollapseWhitespace(UTF16ToWide(text16), true);
  }

  // Try bookmark format.
  //
  // It is tempting to try bookmark format first, but the URL we get out of a
  // bookmark has been cannonicalized via GURL.  This means if a user copies
  // and pastes from the URL bar to itself, the text will get fixed up and
  // cannonicalized, which is not what the user expects.  By pasting in this
  // order, we are sure to paste what the user copied.
  if (clipboard->IsFormatAvailable(Clipboard::GetUrlWFormatType(),
                                   Clipboard::BUFFER_STANDARD)) {
    std::string url_str;
    clipboard->ReadBookmark(NULL, &url_str);
    // pass resulting url string through GURL to normalize
    GURL url(url_str);
    if (url.is_valid()) {
      return UTF8ToWide(url.spec());
    }
  }

  return std::wstring();
}

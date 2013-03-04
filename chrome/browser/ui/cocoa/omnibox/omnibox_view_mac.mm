// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"

#include <Carbon/Carbon.h>  // kVK_Return

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#include "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_popup_view_mac.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/rect.h"

using content::WebContents;

// Focus-handling between |field_| and model() is a bit subtle.
// Other platforms detect change of focus, which is inconvenient
// without subclassing NSTextField (even with a subclass, the use of a
// field editor may complicate things).
//
// model() doesn't actually do anything when it gains focus, it just
// initializes.  Visible activity happens only after the user edits.
// NSTextField delegate receives messages around starting and ending
// edits, so that suffices to catch focus changes.  Since all calls
// into model() start from OmniboxViewMac, in the worst case
// we can add code to sync up the sense of focus as needed.
//
// I've added DCHECK(IsFirstResponder()) in the places which I believe
// should only be reachable when |field_| is being edited.  If these
// fire, it probably means someone unexpected is calling into
// model().
//
// Other platforms don't appear to have the sense of "key window" that
// Mac does (I believe their fields lose focus when the window loses
// focus).  Rather than modifying focus outside the control's edit
// scope, when the window resigns key the autocomplete popup is
// closed.  model() still believes it has focus, and the popup will
// be regenerated on the user's next edit.  That seems to match how
// things work on other platforms.

namespace {

// TODO(shess): This is ugly, find a better way.  Using it right now
// so that I can crib from gtk and still be able to see that I'm using
// the same values easily.
NSColor* ColorWithRGBBytes(int rr, int gg, int bb) {
  DCHECK_LE(rr, 255);
  DCHECK_LE(bb, 255);
  DCHECK_LE(gg, 255);
  return [NSColor colorWithCalibratedRed:static_cast<float>(rr)/255.0
                                   green:static_cast<float>(gg)/255.0
                                    blue:static_cast<float>(bb)/255.0
                                   alpha:1.0];
}

NSColor* HostTextColor() {
  return [NSColor blackColor];
}
NSColor* BaseTextColor() {
  return [NSColor darkGrayColor];
}
NSColor* SecureSchemeColor() {
  return ColorWithRGBBytes(0x07, 0x95, 0x00);
}
NSColor* SecurityErrorSchemeColor() {
  return ColorWithRGBBytes(0xa2, 0x00, 0x00);
}

const char kOmniboxViewMacStateKey[] = "OmniboxViewMacState";

// Store's the model and view state across tab switches.
struct OmniboxViewMacState : public base::SupportsUserData::Data {
  OmniboxViewMacState(const OmniboxEditModel::State model_state,
                      const bool has_focus,
                      const NSRange& selection)
      : model_state(model_state),
        has_focus(has_focus),
        selection(selection) {
  }
  virtual ~OmniboxViewMacState() {}

  const OmniboxEditModel::State model_state;
  const bool has_focus;
  const NSRange selection;
};

// Accessors for storing and getting the state from the tab.
void StoreStateToTab(WebContents* tab,
                     OmniboxViewMacState* state) {
  tab->SetUserData(kOmniboxViewMacStateKey, state);
}
const OmniboxViewMacState* GetStateFromTab(const WebContents* tab) {
  return static_cast<OmniboxViewMacState*>(
      tab->GetUserData(&kOmniboxViewMacStateKey));
}

// Helper to make converting url_parse ranges to NSRange easier to
// read.
NSRange ComponentToNSRange(const url_parse::Component& component) {
  return NSMakeRange(static_cast<NSInteger>(component.begin),
                     static_cast<NSInteger>(component.len));
}

}  // namespace

// static
NSImage* OmniboxViewMac::ImageForResource(int resource_id) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(resource_id).ToNSImage();
}

// static
NSColor* OmniboxViewMac::SuggestTextColor() {
  return [NSColor colorWithCalibratedWhite:0.0 alpha:0.5];
}

OmniboxViewMac::OmniboxViewMac(OmniboxEditController* controller,
                               ToolbarModel* toolbar_model,
                               Profile* profile,
                               CommandUpdater* command_updater,
                               AutocompleteTextField* field)
    : OmniboxView(profile, controller, toolbar_model, command_updater),
      popup_view_(OmniboxPopupViewMac::Create(this, model(), field)),
      field_(field),
      suggest_text_length_(0),
      delete_was_pressed_(false),
      delete_at_end_pressed_(false),
      line_height_(0) {
  [field_ setObserver:this];

  // Needed so that editing doesn't lose the styling.
  [field_ setAllowsEditingTextAttributes:YES];

  // Get the appropriate line height for the font that we use.
  scoped_nsobject<NSLayoutManager>
      layoutManager([[NSLayoutManager alloc] init]);
  [layoutManager setUsesScreenFonts:YES];
  line_height_ = [layoutManager defaultLineHeightForFont:GetFieldFont()];
  DCHECK_GT(line_height_, 0);
}

OmniboxViewMac::~OmniboxViewMac() {
  // Destroy popup view before this object in case it tries to call us
  // back in the destructor.
  popup_view_.reset();

  // Disconnect from |field_|, it outlives this object.
  [field_ setObserver:NULL];
}

void OmniboxViewMac::SaveStateToTab(WebContents* tab) {
  DCHECK(tab);

  const bool hasFocus = [field_ currentEditor] ? true : false;

  NSRange range;
  if (hasFocus) {
    range = GetSelectedRange();
  } else {
    // If we are not focussed, there is no selection.  Manufacture
    // something reasonable in case it starts to matter in the future.
    range = NSMakeRange(0, GetTextLength());
  }

  OmniboxViewMacState* state =
      new OmniboxViewMacState(model()->GetStateForTabSwitch(), hasFocus, range);
  StoreStateToTab(tab, state);
}

void OmniboxViewMac::Update(const WebContents* tab_for_state_restoring) {
  // TODO(shess): It seems like if the tab is non-NULL, then this code
  // shouldn't need to be called at all.  When coded that way, I find
  // that the field isn't always updated correctly.  Figure out why
  // this is.  Maybe this method should be refactored into more
  // specific cases.
  bool user_visible =
      model()->UpdatePermanentText(toolbar_model()->GetText(true));

  if (tab_for_state_restoring) {
    RevertAll();

    const OmniboxViewMacState* state = GetStateFromTab(tab_for_state_restoring);
    if (state) {
      // Should restore the user's text via SetUserText().
      model()->RestoreState(state->model_state);

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
    // code compares the toolbar_model() security level with the local
    // security level.  Dig in and figure out why this isn't a no-op
    // that should go away.
    EmphasizeURLComponents();
  }
}

string16 OmniboxViewMac::GetText() const {
  return base::SysNSStringToUTF16(GetNonSuggestTextSubstring());
}

NSRange OmniboxViewMac::GetSelectedRange() const {
  return [[field_ currentEditor] selectedRange];
}

NSRange OmniboxViewMac::GetMarkedRange() const {
  DCHECK([field_ currentEditor]);
  return [(NSTextView*)[field_ currentEditor] markedRange];
}

void OmniboxViewMac::SetSelectedRange(const NSRange range) {
  // This can be called when we don't have focus.  For instance, when
  // the user clicks the "Go" button.
  if (model()->has_focus()) {
    // TODO(shess): If model() thinks we have focus, this should not
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

void OmniboxViewMac::SetWindowTextAndCaretPos(const string16& text,
                                              size_t caret_pos,
                                              bool update_popup,
                                              bool notify_text_changed) {
  DCHECK_LE(caret_pos, text.size());
  SetTextAndSelectedRange(text, NSMakeRange(caret_pos, 0));

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed)
    TextChanged();
}

void OmniboxViewMac::SetForcedQuery() {
  // We need to do this first, else |SetSelectedRange()| won't work.
  FocusLocation(true);

  const string16 current_text(GetText());
  const size_t start = current_text.find_first_not_of(kWhitespaceUTF16);
  if (start == string16::npos || (current_text[start] != '?')) {
    SetUserText(ASCIIToUTF16("?"));
  } else {
    NSRange range = NSMakeRange(start + 1, current_text.size() - start - 1);
    [[field_ currentEditor] setSelectedRange:range];
  }
}

bool OmniboxViewMac::IsSelectAll() const {
  if (![field_ currentEditor])
    return true;
  const NSRange all_range = NSMakeRange(0, GetTextLength());
  return NSEqualRanges(all_range, GetSelectedRange());
}

bool OmniboxViewMac::DeleteAtEndPressed() {
  return delete_at_end_pressed_;
}

void OmniboxViewMac::GetSelectionBounds(string16::size_type* start,
                                        string16::size_type* end) const {
  if (![field_ currentEditor]) {
    *start = *end = 0;
    return;
  }

  const NSRange selected_range = GetSelectedRange();
  *start = static_cast<size_t>(selected_range.location);
  *end = static_cast<size_t>(NSMaxRange(selected_range));
}

void OmniboxViewMac::SelectAll(bool reversed) {
  // TODO(shess): Figure out what |reversed| implies.  The gtk version
  // has it imply inverting the selection front to back, but I don't
  // even know if that makes sense for Mac.

  // TODO(shess): Verify that we should be stealing focus at this
  // point.
  SetSelectedRange(NSMakeRange(0, GetTextLength()));
}

void OmniboxViewMac::RevertAll() {
  OmniboxView::RevertAll();
  [field_ clearUndoChain];
}

void OmniboxViewMac::UpdatePopup() {
  model()->SetInputInProgress(true);
  if (!model()->has_focus())
    return;

  // Comment copied from OmniboxViewWin::UpdatePopup():
  // Don't inline autocomplete when:
  //   * The user is deleting text
  //   * The caret/selection isn't at the end of the text
  //   * The user has just pasted in something that replaced all the text
  //   * The user is trying to compose something in an IME
  bool prevent_inline_autocomplete = IsImeComposing();
  NSTextView* editor = (NSTextView*)[field_ currentEditor];
  if (editor) {
    if (NSMaxRange([editor selectedRange]) <
        [[editor textStorage] length] - suggest_text_length_)
      prevent_inline_autocomplete = true;
  }

  model()->StartAutocomplete([editor selectedRange].length != 0,
                            prevent_inline_autocomplete);
}

void OmniboxViewMac::CloseOmniboxPopup() {
  // Call both base class methods.
  ClosePopup();
  OmniboxView::CloseOmniboxPopup();
}

void OmniboxViewMac::SetFocus() {
  FocusLocation(false);
  model()->SetCaretVisibility(true);
}

void OmniboxViewMac::ApplyCaretVisibility() {
  [[field_ cell] setHideFocusState:!model()->is_caret_visible()
                            ofView:field_];
}

void OmniboxViewMac::SetText(const string16& display_text) {
  // If we are setting the text directly, there cannot be any suggest text.
  suggest_text_length_ = 0;
  SetTextInternal(display_text);
}

void OmniboxViewMac::SetTextInternal(const string16& display_text) {
  NSString* ss = base::SysUTF16ToNSString(display_text);
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:ss] autorelease];

  // |ApplyTextAttributes()| may call |GetText()|, expecting the current text
  // to be consistent with |suggest_text_length_|, which may not be the case
  // when |SetTextInternal()| is called after updating |suggest_text_length_|.
  // To work around this, set the non-attributed string first, before applying
  // styles to it.
  [field_ setAttributedStringValue:as];

  ApplyTextAttributes(display_text, as);

  [field_ setAttributedStringValue:as];

  // TODO(shess): This may be an appropriate place to call:
  //   model()->OnChanged();
  // In the current implementation, this tells LocationBarViewMac to
  // mess around with model() and update |field_|.  Unfortunately,
  // when I look at our peer implementations, it's not entirely clear
  // to me if this is safe.  SetTextInternal() is sort of an utility method,
  // and different callers sometimes have different needs.  Research
  // this issue so that it can be added safely.

  // TODO(shess): Also, consider whether this code couldn't just
  // manage things directly.  Windows uses a series of overlaid view
  // objects to accomplish the hinting stuff that OnChanged() does, so
  // it makes sense to have it in the controller that lays those
  // things out.  Mac instead pushes the support into a custom
  // text-field implementation.
}

void OmniboxViewMac::SetTextAndSelectedRange(const string16& display_text,
                                             const NSRange range) {
  SetText(display_text);
  SetSelectedRange(range);
}

NSString* OmniboxViewMac::GetNonSuggestTextSubstring() const {
  NSString* text = [field_ stringValue];
  if (suggest_text_length_ > 0) {
    NSUInteger length = [text length];

    DCHECK_LE(suggest_text_length_, length);
    text = [text substringToIndex:(length - suggest_text_length_)];
  }
  return text;
}

NSString* OmniboxViewMac::GetSuggestTextSubstring() const {
  if (suggest_text_length_ == 0)
    return nil;

  NSString* text = [field_ stringValue];
  NSUInteger length = [text length];
  DCHECK_LE(suggest_text_length_, length);
  return [text substringFromIndex:(length - suggest_text_length_)];
}

void OmniboxViewMac::EmphasizeURLComponents() {
  NSTextView* editor = (NSTextView*)[field_ currentEditor];
  // If the autocomplete text field is in editing mode, then we can just change
  // its attributes through its editor. Otherwise, we simply reset its content.
  if (editor) {
    NSTextStorage* storage = [editor textStorage];
    [storage beginEditing];

    // Clear the existing attributes from the text storage, then
    // overlay the appropriate Omnibox attributes.
    [storage setAttributes:[NSDictionary dictionary]
                     range:NSMakeRange(0, [storage length])];
    ApplyTextAttributes(GetText(), storage);

    [storage endEditing];
  } else {
    SetText(GetText());
  }
}

void OmniboxViewMac::ApplyTextAttributes(const string16& display_text,
                                         NSMutableAttributedString* as) {
  NSUInteger as_length = [as length];
  NSRange as_entire_string = NSMakeRange(0, as_length);

  [as addAttribute:NSFontAttributeName value:GetFieldFont()
             range:as_entire_string];

  // A kinda hacky way to add breaking at periods. This is what Safari does.
  // This works for IDNs too, despite the "en_US".
  [as addAttribute:@"NSLanguage" value:@"en_US_POSIX"
             range:as_entire_string];

  // Make a paragraph style locking in the standard line height as the maximum,
  // otherwise the baseline may shift "downwards".
  scoped_nsobject<NSMutableParagraphStyle>
      paragraph_style([[NSMutableParagraphStyle alloc] init]);
  [paragraph_style setMaximumLineHeight:line_height_];
  [as addAttribute:NSParagraphStyleAttributeName value:paragraph_style
             range:as_entire_string];

  // Grey out the suggest text.
  [as addAttribute:NSForegroundColorAttributeName value:SuggestTextColor()
             range:NSMakeRange(as_length - suggest_text_length_,
                               suggest_text_length_)];

  url_parse::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      display_text, model()->GetDesiredTLD(), &scheme, &host);
  const bool emphasize = model()->CurrentTextIsURL() && (host.len > 0);
  if (emphasize) {
    [as addAttribute:NSForegroundColorAttributeName value:BaseTextColor()
               range:as_entire_string];

    [as addAttribute:NSForegroundColorAttributeName value:HostTextColor()
               range:ComponentToNSRange(host)];
  }

  // TODO(shess): GTK has this as a member var, figure out why.
  // [Could it be to not change if no change?  If so, I'm guessing
  // AppKit may already handle that.]
  const ToolbarModel::SecurityLevel security_level =
      toolbar_model()->GetSecurityLevel();

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model()->user_input_in_progress() && model()->CurrentTextIsURL() &&
      scheme.is_nonempty() && (security_level != ToolbarModel::NONE)) {
    NSColor* color;
    if (security_level == ToolbarModel::EV_SECURE ||
        security_level == ToolbarModel::SECURE) {
      color = SecureSchemeColor();
    } else if (security_level == ToolbarModel::SECURITY_ERROR) {
      color = SecurityErrorSchemeColor();
      // Add a strikethrough through the scheme.
      [as addAttribute:NSStrikethroughStyleAttributeName
                 value:[NSNumber numberWithInt:NSUnderlineStyleSingle]
                 range:ComponentToNSRange(scheme)];
    } else if (security_level == ToolbarModel::SECURITY_WARNING) {
      color = BaseTextColor();
    } else {
      NOTREACHED();
      color = BaseTextColor();
    }
    [as addAttribute:NSForegroundColorAttributeName value:color
               range:ComponentToNSRange(scheme)];
  }
}

void OmniboxViewMac::OnTemporaryTextMaybeChanged(const string16& display_text,
                                                 bool save_original_selection,
                                                 bool notify_text_changed) {
  if (save_original_selection)
    saved_temporary_selection_ = GetSelectedRange();

  suggest_text_length_ = 0;
  SetWindowTextAndCaretPos(display_text, display_text.size(), false, false);
  if (notify_text_changed)
    model()->OnChanged();
  [field_ clearUndoChain];
}

void OmniboxViewMac::OnStartingIME() {
  // Reset the suggest text just before starting an IME composition session,
  // otherwise the IME composition may be interrupted when the suggest text
  // gets reset by the IME composition change.
  SetInstantSuggestion(string16());
}

bool OmniboxViewMac::OnInlineAutocompleteTextMaybeChanged(
    const string16& display_text,
    size_t user_text_length) {
  // TODO(shess): Make sure that this actually works.  The round trip
  // to native form and back may mean that it's the same but not the
  // same.
  if (display_text == GetText())
    return false;

  DCHECK_LE(user_text_length, display_text.size());
  const NSRange range =
      NSMakeRange(user_text_length, display_text.size() - user_text_length);
  SetTextAndSelectedRange(display_text, range);
  model()->OnChanged();
  [field_ clearUndoChain];

  return true;
}

void OmniboxViewMac::OnRevertTemporaryText() {
  SetSelectedRange(saved_temporary_selection_);
}

bool OmniboxViewMac::IsFirstResponder() const {
  return [field_ currentEditor] != nil ? true : false;
}

void OmniboxViewMac::OnBeforePossibleChange() {
  // We should only arrive here when the field is focussed.
  DCHECK(IsFirstResponder());

  selection_before_change_ = GetSelectedRange();
  text_before_change_ = GetText();
  marked_range_before_change_ = GetMarkedRange();
}

bool OmniboxViewMac::OnAfterPossibleChange() {
  // We should only arrive here when the field is focussed.
  DCHECK(IsFirstResponder());

  const NSRange new_selection(GetSelectedRange());
  const string16 new_text(GetText());
  const size_t length = new_text.length();

  const bool selection_differs =
      (new_selection.length || selection_before_change_.length) &&
      !NSEqualRanges(new_selection, selection_before_change_);
  const bool at_end_of_edit = (length == new_selection.location);
  const bool text_differs = (new_text != text_before_change_) ||
      !NSEqualRanges(marked_range_before_change_, GetMarkedRange());

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

  delete_at_end_pressed_ = false;

  const bool something_changed = model()->OnAfterPossibleChange(
      text_before_change_, new_text, new_selection.location,
      NSMaxRange(new_selection), selection_differs, text_differs,
      just_deleted_text, !IsImeComposing());

  if (delete_was_pressed_ && at_end_of_edit)
    delete_at_end_pressed_ = true;

  // Restyle in case the user changed something.
  // TODO(shess): I believe there are multiple-redraw cases, here.
  // Linux watches for something_changed && text_differs, but that
  // fails for us in case you copy the URL and paste the identical URL
  // back (we'll lose the styling).
  TextChanged();

  delete_was_pressed_ = false;

  return something_changed;
}

gfx::NativeView OmniboxViewMac::GetNativeView() const {
  return field_;
}

gfx::NativeView OmniboxViewMac::GetRelativeWindowForPopup() const {
  // Not used on mac.
  NOTREACHED();
  return NULL;
}

void OmniboxViewMac::SetInstantSuggestion(const string16& suggest_text) {
  NSString* text = GetNonSuggestTextSubstring();
  bool needs_update = (suggest_text_length_ > 0);

  // Append the new suggest text.
  suggest_text_length_ = suggest_text.length();
  if (suggest_text_length_ > 0) {
    text = [text stringByAppendingString:base::SysUTF16ToNSString(
               suggest_text)];
    needs_update = true;
  }

  if (needs_update) {
    NSRange current_range = GetSelectedRange();
    SetTextInternal(base::SysNSStringToUTF16(text));
    if (NSMaxRange(current_range) <= [text length] - suggest_text_length_)
      SetSelectedRange(current_range);
    else
      SetSelectedRange(NSMakeRange([text length] - suggest_text_length_, 0));
  }
}

string16 OmniboxViewMac::GetInstantSuggestion() const {
  return suggest_text_length_ ?
      base::SysNSStringToUTF16(GetSuggestTextSubstring()) : string16();
}

int OmniboxViewMac::TextWidth() const {
  // Not used on mac.
  NOTREACHED();
  return 0;
}

bool OmniboxViewMac::IsImeComposing() const {
  return [(NSTextView*)[field_ currentEditor] hasMarkedText];
}

void OmniboxViewMac::OnDidBeginEditing() {
  // We should only arrive here when the field is focussed.
  DCHECK([field_ currentEditor]);
}

void OmniboxViewMac::OnBeforeChange() {
  // Capture the current state.
  OnBeforePossibleChange();
}

void OmniboxViewMac::OnDidChange() {
  // Figure out what changed and notify the model.
  OnAfterPossibleChange();
}

void OmniboxViewMac::OnDidEndEditing() {
  ClosePopup();
}

bool OmniboxViewMac::OnDoCommandBySelector(SEL cmd) {
  if (cmd != @selector(moveRight:) &&
      cmd != @selector(insertTab:) &&
      cmd != @selector(insertTabIgnoringFieldEditor:)) {
    // Reset the suggest text for any change other than key right or tab.
    // TODO(rohitrao): This is here to prevent complications when editing text.
    // See if this can be removed.
    SetInstantSuggestion(string16());
  }

  if (cmd == @selector(deleteForward:))
    delete_was_pressed_ = true;

  if (cmd == @selector(moveDown:)) {
    model()->OnUpOrDownKeyPressed(1);
    return true;
  }

  if (cmd == @selector(moveUp:)) {
    model()->OnUpOrDownKeyPressed(-1);
    return true;
  }

  if (model()->popup_model()->IsOpen()) {
    // If instant extended is enabled then allow users to press tab to select
    // results from the omnibox popup.
    BOOL enableTabAutocomplete =
        chrome::search::IsInstantExtendedAPIEnabled(model()->profile());

    if (cmd == @selector(insertBacktab:)) {
      if (model()->popup_model()->selected_line_state() ==
            OmniboxPopupModel::KEYWORD) {
        model()->ClearKeyword(GetText());
        return true;
      } else if (enableTabAutocomplete) {
        model()->OnUpOrDownKeyPressed(-1);
        return true;
      }
    }

    if ((cmd == @selector(insertTab:) ||
        cmd == @selector(insertTabIgnoringFieldEditor:)) &&
        !model()->is_keyword_hint() && enableTabAutocomplete) {
      model()->OnUpOrDownKeyPressed(1);
      return true;
    }
  }

  if (cmd == @selector(moveRight:)) {
    // Only commit suggested text if the cursor is all the way to the right and
    // there is no selection.
    if (suggest_text_length_ > 0 && IsCaretAtEnd()) {
      model()->CommitSuggestedText(true);
      return true;
    }
  }

  if (cmd == @selector(scrollPageDown:)) {
    model()->OnUpOrDownKeyPressed(model()->result().size());
    return true;
  }

  if (cmd == @selector(scrollPageUp:)) {
    model()->OnUpOrDownKeyPressed(-model()->result().size());
    return true;
  }

  if (cmd == @selector(cancelOperation:)) {
    return model()->OnEscapeKeyPressed();
  }

  if ((cmd == @selector(insertTab:) ||
      cmd == @selector(insertTabIgnoringFieldEditor:)) &&
      model()->is_keyword_hint()) {
    return model()->AcceptKeyword();
  }

  // |-noop:| is sent when the user presses Cmd+Return. Override the no-op
  // behavior with the proper WindowOpenDisposition.
  NSEvent* event = [NSApp currentEvent];
  if (cmd == @selector(insertNewline:) ||
     (cmd == @selector(noop:) &&
      ([event type] == NSKeyDown || [event type] == NSKeyUp) &&
      [event keyCode] == kVK_Return)) {
    WindowOpenDisposition disposition =
        event_utils::WindowOpenDispositionFromNSEvent(event);
    model()->AcceptInput(disposition, false);
    // Opening a URL in a background tab should also revert the omnibox contents
    // to their original state.  We cannot do a blanket revert in OpenURL()
    // because middle-clicks also open in a new background tab, but those should
    // not revert the omnibox text.
    RevertAll();
    return true;
  }

  // Option-Return
  if (cmd == @selector(insertNewlineIgnoringFieldEditor:)) {
    model()->AcceptInput(NEW_FOREGROUND_TAB, false);
    return true;
  }

  // When the user does Control-Enter, the existing content has "www."
  // prepended and ".com" appended.  model() should already have
  // received notification when the Control key was depressed, but it
  // is safe to tell it twice.
  if (cmd == @selector(insertLineBreak:)) {
    OnControlKeyChanged(true);
    WindowOpenDisposition disposition =
        event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
    model()->AcceptInput(disposition, false);
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
      if (model()->popup_model()->IsOpen()) {
        model()->popup_model()->TryDeletingCurrentItem();
        return true;
      }
    }
  }

  return false;
}

void OmniboxViewMac::OnSetFocus(bool control_down) {
  model()->OnSetFocus(control_down);
  controller()->OnSetFocus();
}

void OmniboxViewMac::OnKillFocus() {
  // Tell the model to reset itself.
  model()->OnWillKillFocus(NULL);
  model()->OnKillFocus();
  controller()->OnKillFocus();
}

void OmniboxViewMac::OnMouseDown(NSInteger button_number) {
  // Restore caret visibility whenever the user clicks in the the omnibox. This
  // is not always covered by OnSetFocus() because when clicking while the
  // omnibox has invisible focus does not trigger a new OnSetFocus() call.
  if (button_number == 0 || button_number == 1)
    model()->SetCaretVisibility(true);
}

bool OmniboxViewMac::CanCopy() {
  const NSRange selection = GetSelectedRange();
  return selection.length > 0;
}

void OmniboxViewMac::CopyToPasteboard(NSPasteboard* pb) {
  DCHECK(CanCopy());

  const NSRange selection = GetSelectedRange();
  string16 text = base::SysNSStringToUTF16(
      [[field_ stringValue] substringWithRange:selection]);

  // Copy the URL unless this is the search URL and it's being replaced by the
  // Extended Instant API.
  GURL url;
  bool write_url = false;
  if (!ShouldEnableCopyURL()) {
    model()->AdjustTextForCopy(selection.location, IsSelectAll(), &text, &url,
                               &write_url);
  }

  NSString* nstext = base::SysUTF16ToNSString(text);
  [pb declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pb setString:nstext forType:NSStringPboardType];

  if (write_url) {
    [pb declareURLPasteboardWithAdditionalTypes:[NSArray array] owner:nil];
    [pb setDataForURL:base::SysUTF8ToNSString(url.spec()) title:nstext];
  }
  ui::Clipboard::WriteSourceTag(pb,
                                content::BrowserContext::
                                    GetMarkerForOffTheRecordContext(
                                        model()->profile()));
}

void OmniboxViewMac::CopyURLToPasteboard(NSPasteboard* pb) {
  DCHECK(CanCopy());
  DCHECK(ShouldEnableCopyURL());

  string16 text = toolbar_model()->GetText(false);
  GURL url = toolbar_model()->GetURL();

  NSString* nstext = base::SysUTF16ToNSString(text);
  [pb declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pb setString:nstext forType:NSStringPboardType];

  [pb declareURLPasteboardWithAdditionalTypes:[NSArray array] owner:nil];
  [pb setDataForURL:base::SysUTF8ToNSString(url.spec()) title:nstext];
  ui::Clipboard::WriteSourceTag(pb,
                                content::BrowserContext::
                                    GetMarkerForOffTheRecordContext(
                                        model()->profile()));
}

void OmniboxViewMac::OnPaste() {
  // This code currently expects |field_| to be focussed.
  DCHECK([field_ currentEditor]);

  string16 text = GetClipboardText();
  if (text.empty()) {
    return;
  }
  NSString* s = base::SysUTF16ToNSString(text);

  // -shouldChangeTextInRange:* and -didChangeText are documented in
  // NSTextView as things you need to do if you write additional
  // user-initiated editing functions.  They cause the appropriate
  // delegate methods to be called.
  // TODO(shess): It would be nice to separate the Cocoa-specific code
  // from the Chrome-specific code.
  NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
  const NSRange selectedRange = GetSelectedRange();
  if ([editor shouldChangeTextInRange:selectedRange replacementString:s]) {
    // Record this paste, so we can do different behavior.
    model()->on_paste();

    // Force a Paste operation to trigger the text_changed code in
    // OnAfterPossibleChange(), even if identical contents are pasted
    // into the text box.
    text_before_change_.clear();

    [editor replaceCharactersInRange:selectedRange withString:s];
    [editor didChangeText];
  }
}

// TODO(dominich): Move to OmniboxView base class? Currently this is defined on
// the AutocompleteTextFieldObserver but the logic is shared between all
// platforms. Some refactor might be necessary to simplify this. Or at least
// this method could call the OmniboxView version.
bool OmniboxViewMac::ShouldEnableCopyURL() {
  return !model()->user_input_in_progress() &&
      toolbar_model()->WouldReplaceSearchURLWithSearchTerms();
}

bool OmniboxViewMac::CanPasteAndGo() {
  return model()->CanPasteAndGo(GetClipboardText());
}

int OmniboxViewMac::GetPasteActionStringId() {
  string16 text(GetClipboardText());
  DCHECK(model()->CanPasteAndGo(text));
  return model()->IsPasteAndSearch(GetClipboardText()) ?
      IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO;
}

void OmniboxViewMac::OnPasteAndGo() {
  string16 text(GetClipboardText());
  if (model()->CanPasteAndGo(text))
    model()->PasteAndGo(text);
}

void OmniboxViewMac::OnFrameChanged() {
  // TODO(shess): UpdatePopupAppearance() is called frequently, so it
  // should be really cheap, but in this case we could probably make
  // things even cheaper by refactoring between the popup-placement
  // code and the matrix-population code.
  popup_view_->UpdatePopupAppearance();
  model()->OnPopupBoundsChanged(popup_view_->GetTargetBounds());

  // Give controller a chance to rearrange decorations.
  model()->OnChanged();
}

void OmniboxViewMac::ClosePopup() {
  OmniboxView::CloseOmniboxPopup();
}

bool OmniboxViewMac::OnBackspacePressed() {
  // Don't intercept if not in keyword search mode.
  if (model()->is_keyword_hint() || model()->keyword().empty()) {
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
  model()->ClearKeyword(GetText());
  return true;
}

NSRange OmniboxViewMac::SelectionRangeForProposedRange(NSRange proposed_range) {
  // Should never call this function unless editing is in progress.
  DCHECK([field_ currentEditor]);

  if (![field_ currentEditor])
    return proposed_range;

  // Do not use [field_ stringValue] here, as that forces a sync between the
  // field and the editor.  This sync will end up setting the selection, which
  // in turn calls this method, leading to an infinite loop.  Instead, retrieve
  // the current string value directly from the editor.
  size_t text_length = [[[field_ currentEditor] string] length];

  // Cannot select suggested text.
  size_t max = text_length - suggest_text_length_;
  NSUInteger start = proposed_range.location;
  NSUInteger end = proposed_range.location + proposed_range.length;

  if (start > max)
    start = max;

  if (end > max)
    end = max;

  return NSMakeRange(start, end - start);
}

void OmniboxViewMac::OnControlKeyChanged(bool pressed) {
  model()->OnControlKeyChanged(pressed);
}

void OmniboxViewMac::FocusLocation(bool select_all) {
  if ([field_ isEditable]) {
    // If the text field has a field editor, it's the first responder, meaning
    // that it's already focused. makeFirstResponder: will select all, so only
    // call it if this behavior is desired.
    if (select_all || ![field_ currentEditor])
      [[field_ window] makeFirstResponder:field_];
    DCHECK_EQ([field_ currentEditor], [[field_ window] firstResponder]);
  }
}

// static
NSFont* OmniboxViewMac::GetFieldFont() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetFont(ResourceBundle::BaseFont).GetNativeFont();
}

int OmniboxViewMac::GetOmniboxTextLength() const {
  return static_cast<int>(GetTextLength());
}

NSUInteger OmniboxViewMac::GetTextLength() const {
  return ([field_ currentEditor] ?
          [[[field_ currentEditor] string] length] :
          [[field_ stringValue] length]) - suggest_text_length_;
}

bool OmniboxViewMac::IsCaretAtEnd() const {
  const NSRange selection = GetSelectedRange();
  return selection.length == 0 && selection.location == GetTextLength();
}

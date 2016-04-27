// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"

#include <Carbon/Carbon.h>  // kVK_Return

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_editor.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_popup_view_mac.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/security_state/security_state_model.h"
#include "components/toolbar/toolbar_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#import "skia/ext/skia_utils_mac.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"

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

NSColor* HostTextColor(bool inDarkMode) {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    return [NSColor blackColor];
  }
  return inDarkMode ? [NSColor whiteColor] : [NSColor blackColor];
}
NSColor* SecureSchemeColor(bool inDarkMode) {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    return ColorWithRGBBytes(0x07, 0x95, 0x00);
  }
  return inDarkMode ? [NSColor colorWithCalibratedWhite:1 alpha:0.5] :
                      skia::SkColorToCalibratedNSColor(gfx::kGoogleGreen700);
}
NSColor* SecurityWarningSchemeColor(bool inDarkMode) {
  return inDarkMode ? [NSColor colorWithCalibratedWhite:1 alpha:0.5] :
                      skia::SkColorToCalibratedNSColor(gfx::kGoogleYellow700);
}
NSColor* SecurityErrorSchemeColor(bool inDarkMode) {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    return ColorWithRGBBytes(0xa2, 0x00, 0x00);
  }
  return inDarkMode ? [NSColor colorWithCalibratedWhite:1 alpha:0.5] :
                      skia::SkColorToCalibratedNSColor(gfx::kGoogleRed700);
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
  ~OmniboxViewMacState() override {}

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

// Helper to make converting url ranges to NSRange easier to
// read.
NSRange ComponentToNSRange(const url::Component& component) {
  return NSMakeRange(static_cast<NSInteger>(component.begin),
                     static_cast<NSInteger>(component.len));
}

}  // namespace

// static
NSImage* OmniboxViewMac::ImageForResource(int resource_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(resource_id).ToNSImage();
}

// static
NSColor* OmniboxViewMac::SuggestTextColor() {
  return [NSColor colorWithCalibratedWhite:0.0 alpha:0.5];
}

//static
SkColor OmniboxViewMac::BaseTextColorSkia(bool in_dark_mode) {
  return in_dark_mode ? SkColorSetA(SK_ColorWHITE, 0x7F)
                      : SkColorSetA(SK_ColorBLACK, 0x7F);
}

// static
NSColor* OmniboxViewMac::BaseTextColor(bool in_dark_mode) {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    return [NSColor darkGrayColor];
  }
  return skia::SkColorToCalibratedNSColor(BaseTextColorSkia(in_dark_mode));
}

OmniboxViewMac::OmniboxViewMac(OmniboxEditController* controller,
                               Profile* profile,
                               CommandUpdater* command_updater,
                               AutocompleteTextField* field)
    : OmniboxView(
          controller,
          base::WrapUnique(new ChromeOmniboxClient(controller, profile))),
      profile_(profile),
      popup_view_(new OmniboxPopupViewMac(this, model(), field)),
      field_(field),
      saved_temporary_selection_(NSMakeRange(0, 0)),
      selection_before_change_(NSMakeRange(0, 0)),
      marked_range_before_change_(NSMakeRange(0, 0)),
      delete_was_pressed_(false),
      delete_at_end_pressed_(false),
      in_coalesced_update_block_(false),
      do_coalesced_text_update_(false),
      do_coalesced_range_update_(false) {
  [field_ setObserver:this];

  // Needed so that editing doesn't lose the styling.
  [field_ setAllowsEditingTextAttributes:YES];

  // Get the appropriate line height for the font that we use.
  base::scoped_nsobject<NSLayoutManager> layoutManager(
      [[NSLayoutManager alloc] init]);
  [layoutManager setUsesScreenFonts:YES];
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
    // If we are not focused, there is no selection.  Manufacture
    // something reasonable in case it starts to matter in the future.
    range = NSMakeRange(0, GetTextLength());
  }

  OmniboxViewMacState* state =
      new OmniboxViewMacState(model()->GetStateForTabSwitch(), hasFocus, range);
  StoreStateToTab(tab, state);
}

void OmniboxViewMac::OnTabChanged(const WebContents* web_contents) {
  const OmniboxViewMacState* state = GetStateFromTab(web_contents);
  model()->RestoreState(state ? &state->model_state : NULL);
  // Restore focus and selection if they were present when the tab
  // was switched away.
  if (state && state->has_focus) {
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

void OmniboxViewMac::ResetTabState(WebContents* web_contents) {
  StoreStateToTab(web_contents, nullptr);
}

void OmniboxViewMac::Update() {
  if (model()->UpdatePermanentText()) {
    // Something visibly changed.  Re-enable URL replacement.
    controller()->GetToolbarModel()->set_url_replacement_enabled(true);
    model()->UpdatePermanentText();

    const bool was_select_all = IsSelectAll();
    NSTextView* text_view =
        base::mac::ObjCCastStrict<NSTextView>([field_ currentEditor]);
    const bool was_reversed =
        [text_view selectionAffinity] == NSSelectionAffinityUpstream;

    // Restore everything to the baseline look.
    RevertAll();

    // Only select all when we have focus.  If we don't have focus, selecting
    // all is unnecessary since the selection will change on regaining focus,
    // and can in fact cause artifacts, e.g. if the user is on the NTP and
    // clicks a link to navigate, causing |was_select_all| to be vacuously true
    // for the empty omnibox, and we then select all here, leading to the
    // trailing portion of a long URL being scrolled into view.  We could try
    // and address cases like this, but it seems better to just not muck with
    // things when the omnibox isn't focused to begin with.
    if (was_select_all && model()->has_focus())
      SelectAll(was_reversed);
  } else {
    // TODO(shess): This corresponds to _win and _gtk, except those
    // guard it with a test for whether the security level changed.
    // But AFAICT, that can only change if the text changed, and that
    // code compares the toolbar model security level with the local
    // security level.  Dig in and figure out why this isn't a no-op
    // that should go away.
    EmphasizeURLComponents();
  }
}

void OmniboxViewMac::OpenMatch(const AutocompleteMatch& match,
                               WindowOpenDisposition disposition,
                               const GURL& alternate_nav_url,
                               const base::string16& pasted_text,
                               size_t selected_line) {
  // Coalesce text and selection updates from the following function. If we
  // don't do this, the user may see intermediate states as brief flickers.
  in_coalesced_update_block_ = true;
  OmniboxView::OpenMatch(
      match, disposition, alternate_nav_url, pasted_text, selected_line);
  in_coalesced_update_block_ = false;
  if (do_coalesced_text_update_)
    SetText(coalesced_text_update_);
  do_coalesced_text_update_ = false;
  if (do_coalesced_range_update_)
    SetSelectedRange(coalesced_range_update_);
  do_coalesced_range_update_ = false;
}

base::string16 OmniboxViewMac::GetText() const {
  return base::SysNSStringToUTF16([field_ stringValue]);
}

NSRange OmniboxViewMac::GetSelectedRange() const {
  return [[field_ currentEditor] selectedRange];
}

NSRange OmniboxViewMac::GetMarkedRange() const {
  DCHECK([field_ currentEditor]);
  return [(NSTextView*)[field_ currentEditor] markedRange];
}

void OmniboxViewMac::SetSelectedRange(const NSRange range) {
  if (in_coalesced_update_block_) {
    do_coalesced_range_update_ = true;
    coalesced_range_update_ = range;
    return;
  }

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

void OmniboxViewMac::SetWindowTextAndCaretPos(const base::string16& text,
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

  const base::string16 current_text(GetText());
  const size_t start = current_text.find_first_not_of(base::kWhitespaceUTF16);
  if (start == base::string16::npos || (current_text[start] != '?')) {
    SetUserText(base::ASCIIToUTF16("?"));
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

void OmniboxViewMac::GetSelectionBounds(base::string16::size_type* start,
                                        base::string16::size_type* end) const {
  if (![field_ currentEditor]) {
    *start = *end = 0;
    return;
  }

  const NSRange selected_range = GetSelectedRange();
  *start = static_cast<size_t>(selected_range.location);
  *end = static_cast<size_t>(NSMaxRange(selected_range));
}

void OmniboxViewMac::SelectAll(bool reversed) {
  DCHECK(!in_coalesced_update_block_);
  if (!model()->has_focus())
    return;

  NSTextView* text_view =
      base::mac::ObjCCastStrict<NSTextView>([field_ currentEditor]);
  NSSelectionAffinity affinity =
      reversed ? NSSelectionAffinityUpstream : NSSelectionAffinityDownstream;
  NSRange range = NSMakeRange(0, GetTextLength());

  [text_view setSelectedRange:range affinity:affinity stillSelecting:NO];
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
    if (NSMaxRange([editor selectedRange]) < [[editor textStorage] length])
      prevent_inline_autocomplete = true;
  }

  model()->StartAutocomplete([editor selectedRange].length != 0,
                            prevent_inline_autocomplete, false);
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

void OmniboxViewMac::SetText(const base::string16& display_text) {
  SetTextInternal(display_text);
}

void OmniboxViewMac::SetTextInternal(const base::string16& display_text) {
  if (in_coalesced_update_block_) {
    do_coalesced_text_update_ = true;
    coalesced_text_update_ = display_text;
    // Don't do any selection changes, since they apply to the previous text.
    do_coalesced_range_update_ = false;
    return;
  }

  NSString* ss = base::SysUTF16ToNSString(display_text);
  NSMutableAttributedString* attributedString =
      [[[NSMutableAttributedString alloc] initWithString:ss] autorelease];

  ApplyTextAttributes(display_text, attributedString);
  [field_ setAttributedStringValue:attributedString];

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

void OmniboxViewMac::SetTextAndSelectedRange(const base::string16& display_text,
                                             const NSRange range) {
  SetText(display_text);
  SetSelectedRange(range);
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

    // This function can be called during the editor's -resignFirstResponder. If
    // that happens, |storage| and |field_| will not be synced automatically any
    // more. Calling -stringValue ensures that |field_| reflects the changes to
    // |storage|.
    [field_ stringValue];
  } else {
    SetText(GetText());
  }
}

void OmniboxViewMac::ApplyTextStyle(
    NSMutableAttributedString* attributedString) {
  [attributedString addAttribute:NSFontAttributeName
                           value:GetFieldFont(gfx::Font::NORMAL)
                           range:NSMakeRange(0, [attributedString length])];

  // Make a paragraph style locking in the standard line height as the maximum,
  // otherwise the baseline may shift "downwards".
  base::scoped_nsobject<NSMutableParagraphStyle> paragraph_style(
      [[NSMutableParagraphStyle alloc] init]);
  CGFloat line_height = [[field_ cell] lineHeight];
  [paragraph_style setMaximumLineHeight:line_height];
  [paragraph_style setMinimumLineHeight:line_height];
  [paragraph_style setLineBreakMode:NSLineBreakByTruncatingTail];
  [attributedString addAttribute:NSParagraphStyleAttributeName
                           value:paragraph_style
                           range:NSMakeRange(0, [attributedString length])];
}

void OmniboxViewMac::ApplyTextAttributes(
    const base::string16& display_text,
    NSMutableAttributedString* attributedString) {
  NSUInteger as_length = [attributedString length];
  NSRange as_entire_string = NSMakeRange(0, as_length);
  bool inDarkMode = [[field_ window] inIncognitoModeWithSystemTheme];

  ApplyTextStyle(attributedString);

  // A kinda hacky way to add breaking at periods. This is what Safari does.
  // This works for IDNs too, despite the "en_US".
  [attributedString addAttribute:@"NSLanguage"
                           value:@"en_US_POSIX"
                           range:as_entire_string];

  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      display_text, ChromeAutocompleteSchemeClassifier(profile_), &scheme,
      &host);
  bool grey_out_url = display_text.substr(scheme.begin, scheme.len) ==
      base::UTF8ToUTF16(extensions::kExtensionScheme);
  if (model()->CurrentTextIsURL() &&
      (host.is_nonempty() || grey_out_url)) {
    [attributedString addAttribute:NSForegroundColorAttributeName
                             value:BaseTextColor(inDarkMode)
                             range:as_entire_string];

    if (!grey_out_url) {
      [attributedString addAttribute:NSForegroundColorAttributeName
                               value:HostTextColor(inDarkMode)
                               range:ComponentToNSRange(host)];
    }
  }

  // TODO(shess): GTK has this as a member var, figure out why.
  // [Could it be to not change if no change?  If so, I'm guessing
  // AppKit may already handle that.]
  const security_state::SecurityStateModel::SecurityLevel security_level =
      controller()->GetToolbarModel()->GetSecurityLevel(false);

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model()->user_input_in_progress() && model()->CurrentTextIsURL() &&
      scheme.is_nonempty() &&
      (security_level != security_state::SecurityStateModel::NONE)) {
    NSColor* color;
    if (security_level == security_state::SecurityStateModel::EV_SECURE ||
        security_level == security_state::SecurityStateModel::SECURE) {
      color = SecureSchemeColor(inDarkMode);
    } else if (security_level ==
               security_state::SecurityStateModel::SECURITY_ERROR) {
      color = SecurityErrorSchemeColor(inDarkMode);
      // Add a strikethrough through the scheme.
      [attributedString addAttribute:NSStrikethroughStyleAttributeName
                 value:[NSNumber numberWithInt:NSUnderlineStyleSingle]
                 range:ComponentToNSRange(scheme)];
    } else if (security_level ==
               security_state::SecurityStateModel::SECURITY_WARNING) {
      color = SecurityWarningSchemeColor(inDarkMode);
    } else {
      NOTREACHED();
      color = BaseTextColor(inDarkMode);
    }
    [attributedString addAttribute:NSForegroundColorAttributeName
                             value:color
                             range:ComponentToNSRange(scheme)];
  }
}

void OmniboxViewMac::OnTemporaryTextMaybeChanged(
    const base::string16& display_text,
    bool save_original_selection,
    bool notify_text_changed) {
  if (save_original_selection)
    saved_temporary_selection_ = GetSelectedRange();

  SetWindowTextAndCaretPos(display_text, display_text.size(), false, false);
  if (notify_text_changed)
    model()->OnChanged();
  [field_ clearUndoChain];
}

bool OmniboxViewMac::OnInlineAutocompleteTextMaybeChanged(
    const base::string16& display_text,
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

void OmniboxViewMac::OnInlineAutocompleteTextCleared() {
}

void OmniboxViewMac::OnRevertTemporaryText() {
  SetSelectedRange(saved_temporary_selection_);
  // We got here because the user hit the Escape key. We explicitly don't call
  // TextChanged(), since OmniboxPopupModel::ResetToDefaultMatch() has already
  // been called by now, and it would've called TextChanged() if it was
  // warranted.
}

bool OmniboxViewMac::IsFirstResponder() const {
  return [field_ currentEditor] != nil ? true : false;
}

void OmniboxViewMac::OnBeforePossibleChange() {
  // We should only arrive here when the field is focused.
  DCHECK(IsFirstResponder());

  selection_before_change_ = GetSelectedRange();
  text_before_change_ = GetText();
  marked_range_before_change_ = GetMarkedRange();
}

bool OmniboxViewMac::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  // We should only arrive here when the field is focused.
  DCHECK(IsFirstResponder());

  const NSRange new_selection(GetSelectedRange());
  const base::string16 new_text(GetText());
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
  // occurred, rather than intuiting it from context.  Consider whether
  // that would be a stronger approach.
  const bool just_deleted_text =
      (length < text_before_change_.length() &&
       new_selection.location <= selection_before_change_.location);

  delete_at_end_pressed_ = false;

  const bool something_changed = model()->OnAfterPossibleChange(
      text_before_change_, new_text, new_selection.location,
      NSMaxRange(new_selection), selection_differs, text_differs,
      just_deleted_text, allow_keyword_ui_change && !IsImeComposing());

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

void OmniboxViewMac::SetGrayTextAutocompletion(
    const base::string16& suggest_text) {
  if (suggest_text == suggest_text_)
    return;
  suggest_text_ = suggest_text;
  [field_ setGrayTextAutocompletion:base::SysUTF16ToNSString(suggest_text)
                          textColor:SuggestTextColor()];
}

base::string16 OmniboxViewMac::GetGrayTextAutocompletion() const {
  return suggest_text_;
}

int OmniboxViewMac::GetTextWidth() const {
  // Not used on mac.
  NOTREACHED();
  return 0;
}

int OmniboxViewMac::GetWidth() const {
  return ceil([field_ bounds].size.width);
}

bool OmniboxViewMac::IsImeComposing() const {
  return [(NSTextView*)[field_ currentEditor] hasMarkedText];
}

void OmniboxViewMac::OnDidBeginEditing() {
  // We should only arrive here when the field is focused.
  DCHECK([field_ currentEditor]);
}

void OmniboxViewMac::OnBeforeChange() {
  // Capture the current state.
  OnBeforePossibleChange();
}

void OmniboxViewMac::OnDidChange() {
  // Figure out what changed and notify the model.
  OnAfterPossibleChange(true);
}

void OmniboxViewMac::OnDidEndEditing() {
  ClosePopup();
}

void OmniboxViewMac::OnInsertText() {
  // If |insert_char_time_| is not null, there's a pending insert char operation
  // that hasn't been painted yet. Keep the earlier time.
  if (insert_char_time_.is_null())
    insert_char_time_ = base::TimeTicks::Now();
}

void OmniboxViewMac::OnDidDrawRect() {
  if (!insert_char_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Omnibox.CharTypedToRepaintLatency",
                        base::TimeTicks::Now() - insert_char_time_);
    insert_char_time_ = base::TimeTicks();
  }
}

bool OmniboxViewMac::OnDoCommandBySelector(SEL cmd) {
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
    if (cmd == @selector(insertBacktab:)) {
      if (model()->popup_model()->selected_line_state() ==
            OmniboxPopupModel::KEYWORD) {
        model()->ClearKeyword();
        return true;
      } else {
        model()->OnUpOrDownKeyPressed(-1);
        return true;
      }
    }

    if ((cmd == @selector(insertTab:) ||
        cmd == @selector(insertTabIgnoringFieldEditor:)) &&
        !model()->is_keyword_hint()) {
      model()->OnUpOrDownKeyPressed(1);
      return true;
    }
  }

  if (cmd == @selector(moveRight:)) {
    // Only commit suggested text if the cursor is all the way to the right and
    // there is no selection.
    if (suggest_text_.length() > 0 && IsCaretAtEnd()) {
      model()->CommitSuggestedText();
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
    return model()->AcceptKeyword(ENTERED_KEYWORD_MODE_VIA_TAB);
  }

  // |-noop:| is sent when the user presses Cmd+Return. Override the no-op
  // behavior with the proper WindowOpenDisposition.
  NSEvent* event = [NSApp currentEvent];
  if (cmd == @selector(insertNewline:) ||
     (cmd == @selector(noop:) &&
      ([event type] == NSKeyDown || [event type] == NSKeyUp) &&
      [event keyCode] == kVK_Return)) {
    WindowOpenDisposition disposition =
        ui::WindowOpenDispositionFromNSEvent(event);
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
        ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
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
  model()->OnWillKillFocus();
  model()->OnKillFocus();
}

void OmniboxViewMac::OnMouseDown(NSInteger button_number) {
  // Restore caret visibility whenever the user clicks in the the omnibox. This
  // is not always covered by OnSetFocus() because when clicking while the
  // omnibox has invisible focus does not trigger a new OnSetFocus() call.
  if (button_number == 0 || button_number == 1)
    model()->SetCaretVisibility(true);
}

bool OmniboxViewMac::ShouldSelectAllOnMouseDown() {
  return !controller()->GetToolbarModel()->WouldPerformSearchTermReplacement(
      false);
}

bool OmniboxViewMac::CanCopy() {
  const NSRange selection = GetSelectedRange();
  return selection.length > 0;
}

base::scoped_nsobject<NSPasteboardItem> OmniboxViewMac::CreatePasteboardItem() {
  DCHECK(CanCopy());

  const NSRange selection = GetSelectedRange();
  base::string16 text = base::SysNSStringToUTF16(
      [[field_ stringValue] substringWithRange:selection]);

  // Copy the URL unless this is the search URL and it's being replaced by the
  // Extended Instant API.
  GURL url;
  bool write_url = false;
  if (!controller()->GetToolbarModel()->WouldPerformSearchTermReplacement(
      false)) {
    model()->AdjustTextForCopy(selection.location, IsSelectAll(), &text, &url,
                               &write_url);
  }

  if (IsSelectAll())
    UMA_HISTOGRAM_COUNTS(OmniboxEditModel::kCutOrCopyAllTextHistogram, 1);

  NSString* nstext = base::SysUTF16ToNSString(text);
  if (write_url) {
    return ui::ClipboardUtil::PasteboardItemFromUrl(
        base::SysUTF8ToNSString(url.spec()), nstext);
  } else {
    return ui::ClipboardUtil::PasteboardItemFromString(nstext);
  }
}

void OmniboxViewMac::CopyToPasteboard(NSPasteboard* pboard) {
  [pboard clearContents];
  base::scoped_nsobject<NSPasteboardItem> item(CreatePasteboardItem());
  [pboard writeObjects:@[ item.get() ]];
}

void OmniboxViewMac::ShowURL() {
  DCHECK(ShouldEnableShowURL());
  OmniboxView::ShowURL();
}

void OmniboxViewMac::OnPaste() {
  // This code currently expects |field_| to be focused.
  DCHECK([field_ currentEditor]);

  base::string16 text = GetClipboardText();
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
    model()->OnPaste();

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
bool OmniboxViewMac::ShouldEnableShowURL() {
  return controller()->GetToolbarModel()->WouldReplaceURL();
}

bool OmniboxViewMac::CanPasteAndGo() {
  return model()->CanPasteAndGo(GetClipboardText());
}

int OmniboxViewMac::GetPasteActionStringId() {
  base::string16 text(GetClipboardText());
  DCHECK(model()->CanPasteAndGo(text));
  return model()->IsPasteAndSearch(text) ?
      IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO;
}

void OmniboxViewMac::OnPasteAndGo() {
  base::string16 text(GetClipboardText());
  if (model()->CanPasteAndGo(text))
    model()->PasteAndGo(text);
}

void OmniboxViewMac::OnFrameChanged() {
  // TODO(shess): UpdatePopupAppearance() is called frequently, so it
  // should be really cheap, but in this case we could probably make
  // things even cheaper by refactoring between the popup-placement
  // code and the matrix-population code.
  popup_view_->UpdatePopupAppearance();

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
  model()->ClearKeyword();
  return true;
}

NSRange OmniboxViewMac::SelectionRangeForProposedRange(NSRange proposed_range) {
  return proposed_range;
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
NSFont* OmniboxViewMac::GetFieldFont(int style) {
  // This value should be kept in sync with InstantPage::InitializeFonts.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontList(ui::ResourceBundle::BaseFont).Derive(1, style)
      .GetPrimaryFont().GetNativeFont();
}

NSFont* OmniboxViewMac::GetLargeFont(int style) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontList(ui::ResourceBundle::LargeFont)
      .Derive(1, style)
      .GetPrimaryFont()
      .GetNativeFont();
}

NSFont* OmniboxViewMac::GetSmallFont(int style) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontList(ui::ResourceBundle::SmallFont)
      .Derive(1, style)
      .GetPrimaryFont()
      .GetNativeFont();
}

int OmniboxViewMac::GetOmniboxTextLength() const {
  return static_cast<int>(GetTextLength());
}

NSUInteger OmniboxViewMac::GetTextLength() const {
  return [field_ currentEditor] ?  [[[field_ currentEditor] string] length] :
                                   [[field_ stringValue] length];
}

bool OmniboxViewMac::IsCaretAtEnd() const {
  const NSRange selection = GetSelectedRange();
  return NSMaxRange(selection) == GetTextLength();
}

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view_gtk.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

namespace {

const char kTextBaseColor[] = "#808080";
const char kSecureSchemeColor[] = "#009614";
const char kInsecureSchemeColor[] = "#c80000";

size_t GetUTF8Offset(const std::wstring& wide_text, size_t wide_text_offset) {
  return WideToUTF8(wide_text.substr(0, wide_text_offset)).size();
}

// Stores GTK+-specific state so it can be restored after switching tabs.
struct ViewState {
  explicit ViewState(const AutocompleteEditViewGtk::CharRange& selection_range)
      : selection_range(selection_range) {
  }

  // Range of selected text.
  AutocompleteEditViewGtk::CharRange selection_range;
};

struct AutocompleteEditState {
  AutocompleteEditState(const AutocompleteEditModel::State& model_state,
                        const ViewState& view_state)
      : model_state(model_state),
        view_state(view_state) {
  }

  const AutocompleteEditModel::State model_state;
  const ViewState view_state;
};

// Returns a lazily initialized property bag accessor for saving our state in a
// TabContents.
PropertyAccessor<AutocompleteEditState>* GetStateAccessor() {
  static PropertyAccessor<AutocompleteEditState> state;
  return &state;
}

}  // namespace

AutocompleteEditViewGtk::AutocompleteEditViewGtk(
    AutocompleteEditController* controller,
    ToolbarModel* toolbar_model,
    Profile* profile,
    CommandUpdater* command_updater,
    bool popup_window_mode,
    AutocompletePopupPositioner* popup_positioner)
    : text_view_(NULL),
      tag_table_(NULL),
      text_buffer_(NULL),
      base_tag_(NULL),
      secure_scheme_tag_(NULL),
      insecure_scheme_tag_(NULL),
      model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(new AutocompletePopupViewGtk(this, model_.get(), profile,
                                               popup_positioner)),
      controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      popup_window_mode_(popup_window_mode),
      scheme_security_level_(ToolbarModel::NORMAL),
      selection_saved_(false),
      mark_set_handler_id_(0),
      button_1_pressed_(false),
      text_selected_during_click_(false),
      text_view_focused_before_button_press_(false) {
  model_->set_popup_model(popup_view_->GetModel());
}

AutocompleteEditViewGtk::~AutocompleteEditViewGtk() {
  NotificationService::current()->Notify(
      NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
      Source<AutocompleteEditViewGtk>(this),
      NotificationService::NoDetails());

  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
  model_.reset();

  // We own our widget and TextView related objects.
  if (alignment_.get()) {  // Init() has been called.
    alignment_.Destroy();
    g_object_unref(text_buffer_);
    g_object_unref(tag_table_);
    // The tags we created are owned by the tag_table, and should be destroyed
    // along with it.  We don't hold our own reference to them.
  }
}

void AutocompleteEditViewGtk::Init() {
  // The height of the text view is going to change based on the font used.  We
  // don't want to stretch the height, and we want it vertically centered.
  alignment_.Own(gtk_alignment_new(0., 0.5, 1.0, 0.0));

  // The GtkTagTable and GtkTextBuffer are not initially unowned, so we have
  // our own reference when we create them, and we own them.  Adding them to
  // the other objects adds a reference; it doesn't adopt them.
  tag_table_ = gtk_text_tag_table_new();
  text_buffer_ = gtk_text_buffer_new(tag_table_);
  text_view_ = gtk_text_view_new_with_buffer(text_buffer_);
  if (popup_window_mode_)
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view_), false);
  if (browser_defaults::kForceAutocompleteEditFontSize) {
    // Until we switch to vector graphics, force the font size.
    const double kFontSize = 13.4;  // 13.4px == 10pt @ 96dpi
    // On Windows, popups have a font size 5/6 the size of non-popups.
    const double kPopupWindowFontSize = kFontSize * 5.0 / 6.0;
    gtk_util::ForceFontSizePixels(text_view_,
        popup_window_mode_ ? kPopupWindowFontSize : kFontSize);
  }

  // Override the background color for now.  http://crbug.com/12195
  gtk_widget_modify_base(text_view_, GTK_STATE_NORMAL,
      &LocationBarViewGtk::kBackgroundColorByLevel[scheme_security_level_]);

  // The text view was floating.  It will now be owned by the alignment.
  gtk_container_add(GTK_CONTAINER(alignment_.get()), text_view_);

  // TODO(deanm): This will probably have to be handled differently with the
  // tab to search business.  Maybe we should just eat the tab characters.
  // We want the tab key to move focus, not insert a tab.
  gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_view_), false);

  base_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kTextBaseColor, NULL);
  secure_scheme_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kSecureSchemeColor, NULL);
  insecure_scheme_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kInsecureSchemeColor, NULL);
  black_text_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", "#000000", NULL);

  // NOTE: This code used to connect to "changed", however this was fired too
  // often and during bad times (our own buffer changes?).  It works out much
  // better to listen to end-user-action, which should be fired whenever the
  // user makes some sort of change to the buffer.
  g_signal_connect(text_buffer_, "begin-user-action",
                   G_CALLBACK(&HandleBeginUserActionThunk), this);
  g_signal_connect(text_buffer_, "end-user-action",
                   G_CALLBACK(&HandleEndUserActionThunk), this);
  // We connect to key press and release for special handling of a few keys.
  g_signal_connect(text_view_, "key-press-event",
                   G_CALLBACK(&HandleKeyPressThunk), this);
  g_signal_connect(text_view_, "key-release-event",
                   G_CALLBACK(&HandleKeyReleaseThunk), this);
  g_signal_connect(text_view_, "button-press-event",
                   G_CALLBACK(&HandleViewButtonPressThunk), this);
  g_signal_connect(text_view_, "button-release-event",
                   G_CALLBACK(&HandleViewButtonReleaseThunk), this);
  g_signal_connect(text_view_, "focus-in-event",
                   G_CALLBACK(&HandleViewFocusInThunk), this);
  g_signal_connect(text_view_, "focus-out-event",
                   G_CALLBACK(&HandleViewFocusOutThunk), this);
  // NOTE: The GtkTextView documentation asks you not to connect to this
  // signal, but it is very convenient and clean for catching up/down.
  g_signal_connect(text_view_, "move-cursor",
                   G_CALLBACK(&HandleViewMoveCursorThunk), this);
  // Override the size request.  We want to keep the original height request
  // from the widget, since that's font dependent.  We want to ignore the width
  // so we don't force a minimum width based on the text length.
  g_signal_connect(text_view_, "size-request",
                   G_CALLBACK(&HandleViewSizeRequestThunk), this);
  g_signal_connect(text_view_, "populate-popup",
                   G_CALLBACK(&HandlePopulatePopupThunk), this);
  mark_set_handler_id_ = g_signal_connect(
      text_buffer_, "mark-set", G_CALLBACK(&HandleMarkSetThunk), this);
  g_signal_connect(text_view_, "drag-data-received",
                   G_CALLBACK(&HandleDragDataReceivedThunk), this);
}

void AutocompleteEditViewGtk::SetFocus() {
  gtk_widget_grab_focus(text_view_);
}

void AutocompleteEditViewGtk::SaveStateToTab(TabContents* tab) {
  DCHECK(tab);
  GetStateAccessor()->SetProperty(
      tab->property_bag(),
      AutocompleteEditState(model_->GetStateForTabSwitch(),
                            ViewState(GetSelection())));

  // If any text has been selected, register it as the PRIMARY selection so it
  // can still be pasted via middle-click after the text view is cleared.
  if (!selected_text_.empty() && !selection_saved_) {
    SavePrimarySelection(selected_text_);
    selection_saved_ = true;
  }
}

void AutocompleteEditViewGtk::Update(const TabContents* contents) {
  // NOTE: We're getting the URL text here from the ToolbarModel.
  bool visibly_changed_permanent_text =
      model_->UpdatePermanentText(toolbar_model_->GetText());

  ToolbarModel::SecurityLevel security_level =
        toolbar_model_->GetSchemeSecurityLevel();
  bool changed_security_level = (security_level != scheme_security_level_);
  scheme_security_level_ = security_level;

  // TODO(deanm): This doesn't exactly match Windows.  There there is a member
  // background_color_.  I think we can get away with just the level though.
  if (changed_security_level) {
    gtk_widget_modify_base(text_view_, GTK_STATE_NORMAL,
        &LocationBarViewGtk::kBackgroundColorByLevel[security_level]);
  }

  if (contents) {
    selected_text_.clear();
    selection_saved_ = false;
    RevertAll();
    const AutocompleteEditState* state =
        GetStateAccessor()->GetProperty(contents->property_bag());
    if (state) {
      model_->RestoreState(state->model_state);

      // Move the marks for the cursor and the other end of the selection to
      // the previously-saved offsets.
      GtkTextIter selection_iter, insert_iter;
      ItersFromCharRange(
          state->view_state.selection_range, &selection_iter, &insert_iter);
      // TODO(derat): Restore the selection range instead of just the cursor
      // ("insert") position.  This in itself is trivial to do using
      // gtk_text_buffer_select_range(), but then it also becomes necessary to
      // invalidate hidden tabs' saved ranges when another tab or another app
      // takes the selection, lest we incorrectly regrab a stale selection when
      // a hidden tab is later shown.
      gtk_text_buffer_place_cursor(text_buffer_, &insert_iter);
    }
  } else if (visibly_changed_permanent_text) {
    RevertAll();
    // TODO(deanm): There should be code to restore select all here.
  } else if (changed_security_level) {
    EmphasizeURLComponents();
  }
}

void AutocompleteEditViewGtk::OpenURL(const GURL& url,
                                      WindowOpenDisposition disposition,
                                      PageTransition::Type transition,
                                      const GURL& alternate_nav_url,
                                      size_t selected_line,
                                      const std::wstring& keyword) {
  if (!url.is_valid())
    return;

  model_->SendOpenNotification(selected_line, keyword);

  if (disposition != NEW_BACKGROUND_TAB)
    RevertAll();  // Revert the box to its unedited state.
  controller_->OnAutocompleteAccept(url, disposition, transition,
                                    alternate_nav_url);
}

std::wstring AutocompleteEditViewGtk::GetText() const {
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  gchar* utf8 = gtk_text_buffer_get_text(text_buffer_, &start, &end, false);
  std::wstring out(UTF8ToWide(utf8));
  g_free(utf8);
  return out;
}

void AutocompleteEditViewGtk::SetUserText(const std::wstring& text,
                                          const std::wstring& display_text,
                                          bool update_popup) {
  model_->SetUserText(text);
  // TODO(deanm): something about selection / focus change here.
  SetWindowTextAndCaretPos(display_text, display_text.length());
  if (update_popup)
    UpdatePopup();
  TextChanged();
}

void AutocompleteEditViewGtk::SetWindowTextAndCaretPos(const std::wstring& text,
                                                       size_t caret_pos) {
  std::string utf8 = WideToUTF8(text);
  gtk_text_buffer_set_text(text_buffer_, utf8.data(), utf8.length());
  EmphasizeURLComponents();

  GtkTextIter cur_pos;
  gtk_text_buffer_get_iter_at_offset(text_buffer_, &cur_pos, caret_pos);
  gtk_text_buffer_place_cursor(text_buffer_, &cur_pos);
}

void AutocompleteEditViewGtk::SetForcedQuery() {
  const std::wstring current_text(GetText());
  if (current_text.empty() || (current_text[0] != '?')) {
    SetUserText(L"?");
  } else {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
    gtk_text_buffer_get_iter_at_offset(text_buffer_, &start, 1);
    StartUpdatingHighlightedText();
    gtk_text_buffer_select_range(text_buffer_, &start, &end);
    FinishUpdatingHighlightedText();
  }
}

bool AutocompleteEditViewGtk::IsSelectAll() {
  NOTIMPLEMENTED();
  return false;
}

void AutocompleteEditViewGtk::SelectAll(bool reversed) {
  // SelectAll() is invoked as a side effect of other actions (e.g.  switching
  // tabs or hitting Escape) in autocomplete_edit.cc, so we don't update the
  // PRIMARY selection here.
  // TODO(derat): But this is also called by LocationBarView::FocusLocation() --
  // should the X selection be updated when the user hits Ctrl-L?
  SelectAllInternal(reversed, false);
}

void AutocompleteEditViewGtk::RevertAll() {
  ClosePopup();
  model_->Revert();
  TextChanged();
}

void AutocompleteEditViewGtk::UpdatePopup() {
  model_->SetInputInProgress(true);
  if (!model_->has_focus())
    return;

  // Don't inline autocomplete when the caret/selection isn't at the end of
  // the text.
  CharRange sel = GetSelection();
  model_->StartAutocomplete(sel.cp_max < GetTextLength());
}

void AutocompleteEditViewGtk::ClosePopup() {
  popup_view_->GetModel()->StopAutocomplete();
}

void AutocompleteEditViewGtk::OnTemporaryTextMaybeChanged(
    const std::wstring& display_text,
    bool save_original_selection) {
  // TODO(deanm): Ignoring save_original_selection here, etc.
  SetWindowTextAndCaretPos(display_text, display_text.length());
  TextChanged();
}

bool AutocompleteEditViewGtk::OnInlineAutocompleteTextMaybeChanged(
    const std::wstring& display_text,
    size_t user_text_length) {
  if (display_text == GetText())
    return false;

  StartUpdatingHighlightedText();
  SetWindowTextAndCaretPos(display_text, 0);

  // Select the part of the text that was inline autocompleted.
  GtkTextIter bound, insert;
  gtk_text_buffer_get_bounds(text_buffer_, &insert, &bound);
  gtk_text_buffer_get_iter_at_offset(text_buffer_, &insert, user_text_length);
  gtk_text_buffer_select_range(text_buffer_, &insert, &bound);

  FinishUpdatingHighlightedText();
  TextChanged();
  return true;
}

void AutocompleteEditViewGtk::OnRevertTemporaryText() {
  NOTIMPLEMENTED();
}

void AutocompleteEditViewGtk::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
  sel_before_change_ = GetSelection();
}

// TODO(deanm): This is mostly stolen from Windows, and will need some work.
bool AutocompleteEditViewGtk::OnAfterPossibleChange() {
  CharRange new_sel = GetSelection();
  int length = GetTextLength();
  bool selection_differs = (new_sel.cp_min != sel_before_change_.cp_min) ||
                           (new_sel.cp_max != sel_before_change_.cp_max);
  bool at_end_of_edit = (new_sel.cp_min == length && new_sel.cp_max == length);

  // See if the text or selection have changed since OnBeforePossibleChange().
  std::wstring new_text(GetText());
  bool text_differs = (new_text != text_before_change_);

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  bool just_deleted_text =
      (text_before_change_.length() > new_text.length()) &&
      (new_sel.cp_min <= std::min(sel_before_change_.cp_min,
                                 sel_before_change_.cp_max));

  bool something_changed = model_->OnAfterPossibleChange(new_text,
      selection_differs, text_differs, just_deleted_text, at_end_of_edit);

  if (something_changed && text_differs)
    TextChanged();

  return something_changed;
}

void AutocompleteEditViewGtk::HandleBeginUserAction() {
  OnBeforePossibleChange();
}

void AutocompleteEditViewGtk::HandleEndUserAction() {
  // Eat any newline / paragraphs that might have come in, for example in a
  // copy and paste.  We want to make sure our widget stays single line.
  for (;;) {
    GtkTextIter cur;
    gtk_text_buffer_get_start_iter(text_buffer_, &cur);

    // If there is a line ending, this should put us right before the newline
    // or carriage return / newline (or Unicode) sequence.  If not, we're done.
    if (gtk_text_iter_forward_to_line_end(&cur) == FALSE)
      break;

    // Stepping to the next cursor position should put us on the other side of
    // the newline / paragraph / etc sequence, and then delete this range.
    GtkTextIter next_line = cur;
    gtk_text_iter_forward_cursor_position(&next_line);
    gtk_text_buffer_delete(text_buffer_, &cur, &next_line);

    // We've invalidated our iterators, gotta start again.
  }

  OnAfterPossibleChange();
}

gboolean AutocompleteEditViewGtk::HandleKeyPress(GtkWidget* widget,
                                                 GdkEventKey* event) {
  // This is very similar to the special casing of the return key in the
  // GtkTextView key_press default handler.  TODO(deanm): We do however omit
  // some IME related code, this might become a problem if an IME wants to
  // handle enter.  We can get at the im_context and do it ourselves if needed.
  if (event->keyval == GDK_Return ||
      event->keyval == GDK_ISO_Enter ||
      event->keyval == GDK_KP_Enter ||
      event->keyval == GDK_Tab ||
     (event->keyval == GDK_Escape && event->state == 0)) {
    // Handle IME. This is basically taken from GtkTextView and reworked a bit.
    GtkTextIter iter;
    GtkTextView* text_view = GTK_TEXT_VIEW(text_view_);
    GtkTextMark* insert = gtk_text_buffer_get_insert(text_buffer_);
    gtk_text_buffer_get_iter_at_mark(text_buffer_, &iter, insert);
    gboolean can_insert = gtk_text_iter_can_insert(&iter, text_view->editable);
    if (gtk_im_context_filter_keypress(text_view->im_context, event)) {
      // The IME handled it, do the follow up IME handling.
      if (!can_insert) {
        gtk_im_context_reset(text_view->im_context);
      } else {
        text_view->need_im_reset = TRUE;
      }
    } else {
      // Ok, not handled by the IME, we can handle it.
      if (event->keyval == GDK_Tab) {
        if (model_->is_keyword_hint() && !model_->keyword().empty()) {
          model_->AcceptKeyword();
        } else {
          return FALSE;  // Let GtkTextView handle the tab focus change.
        }
      } else if (event->keyval == GDK_Escape) {
        model_->OnEscapeKeyPressed();
      } else {
        bool alt_held = (event->state & GDK_MOD1_MASK);
        model_->AcceptInput(alt_held ? NEW_FOREGROUND_TAB : CURRENT_TAB, false);
      }
    }
    return TRUE;  // Don't propagate into GtkTextView.
  }

  return FALSE;  // Propagate into GtkTextView.
}

gboolean AutocompleteEditViewGtk::HandleKeyRelease(GtkWidget* widget,
                                                   GdkEventKey* event) {
  // Even though we handled the press ourselves, let GtkTextView handle the
  // release.  It shouldn't do anything particularly interesting, but it will
  // handle the IME work for us.
  return FALSE;  // Propagate into GtkTextView.
}

gboolean AutocompleteEditViewGtk::HandleViewButtonPress(GdkEventButton* event) {
  if (event->button == 1) {
    // When the first button is pressed, track some stuff that will help us
    // determine whether we should select all of the text when the button is
    // released.
    button_1_pressed_ = true;
    text_view_focused_before_button_press_ = GTK_WIDGET_HAS_FOCUS(text_view_);
    text_selected_during_click_ = false;
  }
  return FALSE;
}

gboolean AutocompleteEditViewGtk::HandleViewButtonRelease(
    GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  button_1_pressed_ = false;

  // Call the GtkTextView default handler, ignoring the fact that it will
  // likely have told us to stop propagating.  We want to handle selection.
  GtkWidgetClass* klass = GTK_WIDGET_GET_CLASS(text_view_);
  klass->button_release_event(text_view_, event);

  if (!text_view_focused_before_button_press_ && !text_selected_during_click_) {
    // If this was a focusing click and the user didn't drag to highlight any
    // text, select the full input and update the PRIMARY selection.
    SelectAllInternal(false, true);

    // So we told the buffer where the cursor should be, but make sure to tell
    // the view so it can scroll it to be visible if needed.
    // NOTE: This function doesn't seem to like a count of 0, looking at the
    // code it will skip an important loop.  Use -1 to achieve the same.
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
    gtk_text_view_move_visually(GTK_TEXT_VIEW(text_view_), &start, -1);
  }

  return TRUE;  // Don't continue, we called the default handler already.
}

gboolean AutocompleteEditViewGtk::HandleViewFocusIn() {
  model_->OnSetFocus(false);
  // TODO(deanm): Some keyword hit business, etc here.

  return FALSE;  // Continue propagation.
}

gboolean AutocompleteEditViewGtk::HandleViewFocusOut() {
  // Close the popup.
  ClosePopup();
  // Tell the model to reset itself.
  model_->OnKillFocus();
  return FALSE;  // Pass the event on to the GtkTextView.
}

void AutocompleteEditViewGtk::HandleViewMoveCursor(
    GtkMovementStep step,
    gint count,
    gboolean extendion_selection) {
  // Handle up/down/pgup/pgdn movement on our own.
  int move_amount = count;
  if (step == GTK_MOVEMENT_PAGES)
    move_amount = model_->result().size() * ((count < 0) ? -1 : 1);
  else if (step != GTK_MOVEMENT_DISPLAY_LINES)
    return;  // Propagate into GtkTextView
  model_->OnUpOrDownKeyPressed(move_amount);
  // move-cursor doesn't use a signal accumulator on the return value (it
  // just ignores then), so we have to stop the propagation.
  g_signal_stop_emission_by_name(text_view_, "move-cursor");
}

void AutocompleteEditViewGtk::HandleViewSizeRequest(GtkRequisition* req) {
  // Don't force a minimum width, but use the font-relative height.  This is a
  // run-first handler, so the default handler was already called.
  req->width = 1;
}

void AutocompleteEditViewGtk::HandlePopulatePopup(GtkMenu* menu) {
  GtkWidget* separator = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  gtk_widget_show(separator);

  // Search Engine menu item.
  GtkWidget* search_engine_menuitem = gtk_menu_item_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_EDIT_SEARCH_ENGINES)).c_str());
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), search_engine_menuitem);
  g_signal_connect(search_engine_menuitem, "activate",
                   G_CALLBACK(HandleEditSearchEnginesThunk), this);
  gtk_widget_show(search_engine_menuitem);

  // Paste and Go menu item.
  GtkWidget* paste_go_menuitem = gtk_menu_item_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(model_->is_paste_and_search() ?
              IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO)).c_str());
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_go_menuitem);
  g_signal_connect(paste_go_menuitem, "activate",
                   G_CALLBACK(HandlePasteAndGoThunk), this);
  gtk_widget_show(paste_go_menuitem);
}

void AutocompleteEditViewGtk::HandleEditSearchEngines() {
  command_updater_->ExecuteCommand(IDC_EDIT_SEARCH_ENGINES);
}

void AutocompleteEditViewGtk::HandlePasteAndGo() {
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_text(x_clipboard, HandlePasteAndGoReceivedTextThunk,
                             this);
}

void AutocompleteEditViewGtk::HandlePasteAndGoReceivedText(
    const std::wstring& text) {
  if (model_->CanPasteAndGo(text))
    model_->PasteAndGo();
}

void AutocompleteEditViewGtk::HandleMarkSet(GtkTextBuffer* buffer,
                                            GtkTextIter* location,
                                            GtkTextMark* mark) {
  if (!text_buffer_ || buffer != text_buffer_)
    return;

  if (mark != gtk_text_buffer_get_insert(text_buffer_) &&
      mark != gtk_text_buffer_get_selection_bound(text_buffer_)) {
    return;
  }

  // Is no text selected in the GtkTextView?
  bool no_text_selected = false;

  // Get the currently-selected text, if there is any.
  GtkTextIter start, end;
  if (!gtk_text_buffer_get_selection_bounds(text_buffer_, &start, &end)) {
    no_text_selected = true;
  } else {
    gchar* text = gtk_text_iter_get_text(&start, &end);
    size_t text_len = strlen(text);
    if (!text_len) {
      no_text_selected = true;
    } else {
      selected_text_ = std::string(text, text_len);
      selection_saved_ = false;
    }
    g_free(text);
  }

  // If the user just selected some text with the mouse (or at least while the
  // mouse button was down), make sure that we won't blow their selection away
  // later by selecting all of the text when the button is released.
  if (button_1_pressed_ && !no_text_selected) {
    text_selected_during_click_ = true;
  }

  // If we have some previously-selected text but it's no longer highlighted
  // and we haven't saved it as the selection yet, we save it now.
  if (!selected_text_.empty() && no_text_selected && !selection_saved_) {
    SavePrimarySelection(selected_text_);
    selection_saved_ = true;
  }
}

// Just use the default behavior for DnD, except if the drop can be a PasteAndGo
// then override.
void AutocompleteEditViewGtk::HandleDragDataReceived(
    GdkDragContext* context, gint x, gint y,
    GtkSelectionData* selection_data, guint target_type, guint time) {
  // Don't try to PasteAndGo on drops originating from this omnibox. However, do
  // allow default behavior for such drags.
  if (context->source_window == text_view_->window)
    return;

  guchar* text = gtk_selection_data_get_text(selection_data);
  if (!text)
    return;

  std::wstring possible_url = UTF8ToWide(reinterpret_cast<char*>(text));
  g_free(text);
  if (model_->CanPasteAndGo(CollapseWhitespace(possible_url, true))) {
    model_->PasteAndGo();
    gtk_drag_finish(context, TRUE, TRUE, time);
    g_signal_stop_emission_by_name(text_view_, "drag-data-received");
  }
}

void AutocompleteEditViewGtk::SelectAllInternal(bool reversed,
                                                bool update_primary_selection) {
  GtkTextIter start, end;
  if (reversed) {
    gtk_text_buffer_get_bounds(text_buffer_, &end, &start);
  } else {
    gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  }
  if (!update_primary_selection)
    StartUpdatingHighlightedText();
  gtk_text_buffer_select_range(text_buffer_, &start, &end);
  if (!update_primary_selection)
    FinishUpdatingHighlightedText();
}

void AutocompleteEditViewGtk::StartUpdatingHighlightedText() {
  if (GTK_WIDGET_REALIZED(text_view_)) {
    GtkClipboard* clipboard =
        gtk_widget_get_clipboard(text_view_, GDK_SELECTION_PRIMARY);
    DCHECK(clipboard);
    if (clipboard)
      gtk_text_buffer_remove_selection_clipboard(text_buffer_, clipboard);
  }
  g_signal_handler_block(text_buffer_, mark_set_handler_id_);
}

void AutocompleteEditViewGtk::FinishUpdatingHighlightedText() {
  if (GTK_WIDGET_REALIZED(text_view_)) {
    GtkClipboard* clipboard =
        gtk_widget_get_clipboard(text_view_, GDK_SELECTION_PRIMARY);
    DCHECK(clipboard);
    if (clipboard)
      gtk_text_buffer_add_selection_clipboard(text_buffer_, clipboard);
  }
  g_signal_handler_unblock(text_buffer_, mark_set_handler_id_);
}

AutocompleteEditViewGtk::CharRange AutocompleteEditViewGtk::GetSelection() {
  // You can not just use get_selection_bounds here, since the order will be
  // ascending, and you don't know where the user's start and end of the
  // selection was (if the selection was forwards or backwards).  Get the
  // actual marks so that we can preserve the selection direction.
  GtkTextIter start, insert;
  GtkTextMark* mark;

  mark = gtk_text_buffer_get_selection_bound(text_buffer_);
  gtk_text_buffer_get_iter_at_mark(text_buffer_, &start, mark);

  mark = gtk_text_buffer_get_insert(text_buffer_);
  gtk_text_buffer_get_iter_at_mark(text_buffer_, &insert, mark);

  return CharRange(gtk_text_iter_get_offset(&start),
                   gtk_text_iter_get_offset(&insert));
}

void AutocompleteEditViewGtk::ItersFromCharRange(const CharRange& range,
                                                 GtkTextIter* iter_min,
                                                 GtkTextIter* iter_max) {
  gtk_text_buffer_get_iter_at_offset(text_buffer_, iter_min, range.cp_min);
  gtk_text_buffer_get_iter_at_offset(text_buffer_, iter_max, range.cp_max);
}

int AutocompleteEditViewGtk::GetTextLength() {
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  return gtk_text_iter_get_offset(&end);
}

void AutocompleteEditViewGtk::EmphasizeURLComponents() {
  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  url_parse::Parsed parts;
  std::wstring text = GetText();
  AutocompleteInput::Parse(text, model_->GetDesiredTLD(), &parts, NULL);
  bool emphasize = model_->CurrentTextIsURL() && (parts.host.len > 0);

  // Set the baseline emphasis.
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  gtk_text_buffer_remove_all_tags(text_buffer_, &start, &end);
  if (emphasize) {
    gtk_text_buffer_apply_tag(text_buffer_, base_tag_, &start, &end);

    // We've found a host name, give it more emphasis.
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &start, 0,
                                           GetUTF8Offset(text,
                                                         parts.host.begin));
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &end, 0,
                                           GetUTF8Offset(text,
                                                         parts.host.end()));
    // The following forces the text color to black.  When we start obeying the
    // user theme, we want to remove_all_tags (to get the user's default
    // text color) rather than applying a color tag.  http://crbug.com/12195
    gtk_text_buffer_apply_tag(text_buffer_, black_text_tag_, &start, &end);
  } else {
    // For now, force the text color to be black.  Eventually, we should allow
    // the user to override via gtk theming.  http://crbug.com/12195
    gtk_text_buffer_apply_tag(text_buffer_, black_text_tag_, &start, &end);
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model_->user_input_in_progress() && parts.scheme.is_nonempty() &&
      (scheme_security_level_ != ToolbarModel::NORMAL)) {
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &start, 0,
                                           GetUTF8Offset(text,
                                                         parts.scheme.begin));
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &end, 0,
                                           GetUTF8Offset(text,
                                                         parts.scheme.end()));
    if (scheme_security_level_ == ToolbarModel::SECURE) {
      gtk_text_buffer_apply_tag(text_buffer_, secure_scheme_tag_,
                                &start, &end);
    } else {
      gtk_text_buffer_apply_tag(text_buffer_, insecure_scheme_tag_,
                                &start, &end);
    }
  }
}

void AutocompleteEditViewGtk::TextChanged() {
  EmphasizeURLComponents();
  controller_->OnChanged();
}

void AutocompleteEditViewGtk::SavePrimarySelection(
    const std::string& selected_text) {
  GtkClipboard* clipboard =
      gtk_widget_get_clipboard(text_view_, GDK_SELECTION_PRIMARY);
  DCHECK(clipboard);
  if (!clipboard)
    return;

  gtk_clipboard_set_text(
      clipboard, selected_text.data(), selected_text.size());
}

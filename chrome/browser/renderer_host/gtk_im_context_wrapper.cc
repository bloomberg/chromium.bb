// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/gtk_im_context_wrapper.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <algorithm>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#if !defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/gtk/menu_gtk.h"
#endif
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "gfx/gtk_util.h"
#include "gfx/rect.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {
// Copied from third_party/WebKit/Source/WebCore/page/EventHandler.cpp
//
// Match key code of composition keydown event on windows.
// IE sends VK_PROCESSKEY which has value 229;
//
// Please refer to following documents for detals:
// - Virtual-Key Codes
//   http://msdn.microsoft.com/en-us/library/ms645540(VS.85).aspx
// - How the IME System Works
//   http://msdn.microsoft.com/en-us/library/cc194848.aspx
// - ImmGetVirtualKey Function
//   http://msdn.microsoft.com/en-us/library/dd318570(VS.85).aspx
const int kCompositionEventKeyCode = 229;
}  // namespace

GtkIMContextWrapper::GtkIMContextWrapper(RenderWidgetHostViewGtk* host_view)
    : host_view_(host_view),
      context_(gtk_im_multicontext_new()),
      context_simple_(gtk_im_context_simple_new()),
      is_focused_(false),
      is_composing_text_(false),
      is_enabled_(false),
      is_in_key_event_handler_(false),
      preedit_selection_start_(0),
      preedit_selection_end_(0),
      is_preedit_changed_(false),
      suppress_next_commit_(false),
      last_key_code_(0),
      last_key_was_up_(false),
      last_key_filtered_no_result_(false) {
  DCHECK(context_);
  DCHECK(context_simple_);

  // context_ and context_simple_ share the same callback handlers.
  // All data come from them are treated equally.
  // context_ is for full input method support.
  // context_simple_ is for supporting dead/compose keys when input method is
  // disabled by webkit, eg. in password input box.
  g_signal_connect(context_, "preedit_start",
                   G_CALLBACK(HandlePreeditStartThunk), this);
  g_signal_connect(context_, "preedit_end",
                   G_CALLBACK(HandlePreeditEndThunk), this);
  g_signal_connect(context_, "preedit_changed",
                   G_CALLBACK(HandlePreeditChangedThunk), this);
  g_signal_connect(context_, "commit",
                   G_CALLBACK(HandleCommitThunk), this);

  g_signal_connect(context_simple_, "preedit_start",
                   G_CALLBACK(HandlePreeditStartThunk), this);
  g_signal_connect(context_simple_, "preedit_end",
                   G_CALLBACK(HandlePreeditEndThunk), this);
  g_signal_connect(context_simple_, "preedit_changed",
                   G_CALLBACK(HandlePreeditChangedThunk), this);
  g_signal_connect(context_simple_, "commit",
                   G_CALLBACK(HandleCommitThunk), this);

  GtkWidget* widget = host_view->native_view();
  DCHECK(widget);

  g_signal_connect(widget, "realize",
                   G_CALLBACK(HandleHostViewRealizeThunk), this);
  g_signal_connect(widget, "unrealize",
                   G_CALLBACK(HandleHostViewUnrealizeThunk), this);

  // Set client window if the widget is already realized.
  HandleHostViewRealize(widget);
}

GtkIMContextWrapper::~GtkIMContextWrapper() {
  if (context_)
    g_object_unref(context_);
  if (context_simple_)
    g_object_unref(context_simple_);
}

void GtkIMContextWrapper::ProcessKeyEvent(GdkEventKey* event) {
  suppress_next_commit_ = false;

  // Indicates preedit-changed and commit signal handlers that we are
  // processing a key event.
  is_in_key_event_handler_ = true;
  // Reset this flag so that we can know if preedit is changed after
  // processing this key event.
  is_preedit_changed_ = false;
  // Clear it so that we can know if something needs committing after
  // processing this key event.
  commit_text_.clear();

  // According to Document Object Model (DOM) Level 3 Events Specification
  // Appendix A: Keyboard events and key identifiers
  // http://www.w3.org/TR/DOM-Level-3-Events/keyset.html:
  // The event sequence would be:
  // 1. keydown
  // 2. textInput
  // 3. keyup
  //
  // So keydown must be sent to webkit before sending input method result,
  // while keyup must be sent afterwards.
  // However on Windows, if a keydown event has been processed by IME, its
  // virtual keycode will be changed to VK_PROCESSKEY(0xE5) before being sent
  // to application.
  // To emulate the windows behavior as much as possible, we need to send the
  // key event to the GtkIMContext object first, and decide whether or not to
  // send the original key event to webkit according to the result from IME.
  //
  // If IME is enabled by WebKit, this event will be dispatched to context_
  // to get full IME support. Otherwise it'll be dispatched to
  // context_simple_, so that dead/compose keys can still work.
  //
  // It sends a "commit" signal when it has a character to be inserted
  // even when we use a US keyboard so that we can send a Char event
  // (or an IME event) to the renderer in our "commit"-signal handler.
  // We should send a KeyDown (or a KeyUp) event before dispatching this
  // event to the GtkIMContext object (and send a Char event) so that WebKit
  // can dispatch the JavaScript events in the following order: onkeydown(),
  // onkeypress(), and onkeyup(). (Many JavaScript pages assume this.)
  gboolean filtered = false;
  if (is_enabled_) {
    filtered = gtk_im_context_filter_keypress(context_, event);
  } else {
    filtered = gtk_im_context_filter_keypress(context_simple_, event);
  }

  // Reset this flag here, as it's only used in input method callbacks.
  is_in_key_event_handler_ = false;

  NativeWebKeyboardEvent wke(event);

  // If the key event was handled by the input method, then we need to prevent
  // RenderView::UnhandledKeyboardEvent() from processing it.
  // Otherwise unexpected result may occur. For example if it's a
  // Backspace key event, the browser may go back to previous page.
  // We just send all keyup events to the browser to avoid breaking the
  // browser's MENU key function, which is actually the only keyup event
  // handled in the browser.
  if (filtered && event->type == GDK_KEY_PRESS)
    wke.skip_in_browser = true;

  const int key_code = wke.windowsKeyCode;
  const bool has_result = HasInputMethodResult();

  // Send filtered keydown event before sending IME result.
  // In order to workaround http://crosbug.com/6582, we only send a filtered
  // keydown event if it generated any input method result.
  if (event->type == GDK_KEY_PRESS && filtered && has_result)
    ProcessFilteredKeyPressEvent(&wke);

  // Send IME results. In most cases, it's only available if the key event
  // is filtered by IME. But in rare cases, an unfiltered key event may also
  // generate IME results.
  // Any IME results generated by a unfiltered key down event must be sent
  // before the key down event, to avoid some tricky issues. For example,
  // when using latin-post input method, pressing 'a' then Backspace, may
  // generate following events in sequence:
  //  1. keydown 'a' (filtered)
  //  2. preedit changed to "a"
  //  3. keyup 'a' (unfiltered)
  //  4. keydown Backspace (unfiltered)
  //  5. commit "a"
  //  6. preedit end
  //  7. keyup Backspace (unfiltered)
  //
  // In this case, the input box will be in a strange state if keydown
  // Backspace is sent to webkit before commit "a" and preedit end.
  if (has_result)
    ProcessInputMethodResult(event, filtered);

  // Send unfiltered keydown and keyup events after sending IME result.
  if (event->type == GDK_KEY_PRESS && !filtered) {
    ProcessUnfilteredKeyPressEvent(&wke);
  } else if (event->type == GDK_KEY_RELEASE) {
    // In order to workaround http://crosbug.com/6582, we need to suppress
    // the keyup event if corresponding keydown event was suppressed, or
    // the last key event was a keyup event with the same keycode.
    const bool suppress = (last_key_code_ == key_code) &&
        (last_key_was_up_ || last_key_filtered_no_result_);

    if (!suppress)
      host_view_->ForwardKeyboardEvent(wke);
  }

  last_key_code_ = key_code;
  last_key_was_up_ = (event->type == GDK_KEY_RELEASE);
  last_key_filtered_no_result_ = (filtered && !has_result);
}

void GtkIMContextWrapper::UpdateInputMethodState(WebKit::WebTextInputType type,
                                                 const gfx::Rect& caret_rect) {
  suppress_next_commit_ = false;

  // The renderer has updated its IME status.
  // Control the GtkIMContext object according to this status.
  if (!context_ || !is_focused_)
    return;

  DCHECK(!is_in_key_event_handler_);

  bool is_enabled = (type == WebKit::WebTextInputTypeText);
  if (is_enabled_ != is_enabled) {
    is_enabled_ = is_enabled;
    if (is_enabled)
      gtk_im_context_focus_in(context_);
    else
      gtk_im_context_focus_out(context_);
  }

  if (is_enabled) {
    // Updates the position of the IME candidate window.
    // The position sent from the renderer is a relative one, so we need to
    // attach the GtkIMContext object to this window before changing the
    // position.
    GdkRectangle cursor_rect(caret_rect.ToGdkRectangle());
    gtk_im_context_set_cursor_location(context_, &cursor_rect);
  }
}

void GtkIMContextWrapper::OnFocusIn() {
  if (is_focused_)
    return;

  // Tracks the focused state so that we can give focus to the
  // GtkIMContext object correctly later when IME is enabled by WebKit.
  is_focused_ = true;

  last_key_code_ = 0;
  last_key_was_up_ = false;
  last_key_filtered_no_result_ = false;

  // Notify the GtkIMContext object of this focus-in event only if IME is
  // enabled by WebKit.
  if (is_enabled_)
    gtk_im_context_focus_in(context_);

  // context_simple_ is always enabled.
  // Actually it doesn't care focus state at all.
  gtk_im_context_focus_in(context_simple_);

  // Enables RenderWidget's IME related events, so that we can be notified
  // when WebKit wants to enable or disable IME.
  if (host_view_->GetRenderWidgetHost())
    host_view_->GetRenderWidgetHost()->SetInputMethodActive(true);
}

void GtkIMContextWrapper::OnFocusOut() {
  if (!is_focused_)
    return;

  // Tracks the focused state so that we won't give focus to the
  // GtkIMContext object unexpectly.
  is_focused_ = false;

  // Notify the GtkIMContext object of this focus-out event only if IME is
  // enabled by WebKit.
  if (is_enabled_) {
    // To reset the GtkIMContext object and prevent data loss.
    ConfirmComposition();
    gtk_im_context_focus_out(context_);
  }

  // To make sure it'll be in correct state when focused in again.
  gtk_im_context_reset(context_simple_);
  gtk_im_context_focus_out(context_simple_);

  is_composing_text_ = false;

  // Disable RenderWidget's IME related events to save bandwidth.
  if (host_view_->GetRenderWidgetHost())
    host_view_->GetRenderWidgetHost()->SetInputMethodActive(false);
}

#if !defined(TOOLKIT_VIEWS)
// Not defined for views because the views context menu doesn't
// implement input methods yet.
void GtkIMContextWrapper::AppendInputMethodsContextMenu(MenuGtk* menu) {
  gboolean show_input_method_menu = TRUE;

  g_object_get(gtk_widget_get_settings(GTK_WIDGET(host_view_->native_view())),
               "gtk-show-input-method-menu", &show_input_method_menu, NULL);
  if (!show_input_method_menu)
    return;

  std::string label = gfx::ConvertAcceleratorsFromWindowsStyle(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_INPUT_METHODS_MENU));
  GtkWidget* menuitem = gtk_menu_item_new_with_mnemonic(label.c_str());
  GtkWidget* submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
  gtk_im_multicontext_append_menuitems(GTK_IM_MULTICONTEXT(context_),
                                       GTK_MENU_SHELL(submenu));
  menu->AppendSeparator();
  menu->AppendMenuItem(IDC_INPUT_METHODS_MENU, menuitem);
}
#endif

void GtkIMContextWrapper::CancelComposition() {
  if (!is_enabled_)
    return;

  DCHECK(!is_in_key_event_handler_);

  // To prevent any text from being committed when resetting the |context_|;
  is_in_key_event_handler_ = true;
  suppress_next_commit_ = true;

  gtk_im_context_reset(context_);
  gtk_im_context_reset(context_simple_);

  if (is_focused_) {
    // Some input methods may not honour the reset call. Focusing out/in the
    // |context_| to make sure it gets reset correctly.
    gtk_im_context_focus_out(context_);
    gtk_im_context_focus_in(context_);
  }

  is_composing_text_ = false;
  preedit_text_.clear();
  preedit_underlines_.clear();
  commit_text_.clear();

  is_in_key_event_handler_ = false;
}

bool GtkIMContextWrapper::NeedCommitByForwardingCharEvent() const {
  // If there is no composition text and has only one character to be
  // committed, then the character will be send to webkit as a Char event
  // instead of a confirmed composition text.
  // It should be fine to handle BMP character only, as non-BMP characters
  // can always be committed as confirmed composition text.
  return !is_composing_text_ && commit_text_.length() == 1;
}

bool GtkIMContextWrapper::HasInputMethodResult() const {
  return commit_text_.length() || is_preedit_changed_;
}

void GtkIMContextWrapper::ProcessFilteredKeyPressEvent(
    NativeWebKeyboardEvent* wke) {
  // If IME has filtered this event, then replace virtual key code with
  // VK_PROCESSKEY. See comment in ProcessKeyEvent() for details.
  // It's only required for keydown events.
  // To emulate windows behavior, when input method is enabled, if the commit
  // text can be emulated by a Char event, then don't do this replacement.
  if (!NeedCommitByForwardingCharEvent()) {
    wke->windowsKeyCode = kCompositionEventKeyCode;
    // keyidentifier must be updated accordingly, otherwise this key event may
    // still be processed by webkit.
    wke->setKeyIdentifierFromWindowsKeyCode();
  }
  host_view_->ForwardKeyboardEvent(*wke);
}

void GtkIMContextWrapper::ProcessUnfilteredKeyPressEvent(
    NativeWebKeyboardEvent* wke) {
  // Send keydown event as it, because it's not filtered by IME.
  host_view_->ForwardKeyboardEvent(*wke);

  // IME is disabled by WebKit or the GtkIMContext object cannot handle
  // this key event.
  // This case is caused by two reasons:
  // 1. The given key event is a control-key event, (e.g. return, page up,
  //    page down, tab, arrows, etc.) or;
  // 2. The given key event is not a control-key event but printable
  //    characters aren't assigned to the event, (e.g. alt+d, etc.)
  // Create a Char event manually from this key event and send it to the
  // renderer when this Char event contains a printable character which
  // should be processed by WebKit.
  // isSystemKey will be set to true if this key event has Alt modifier,
  // see WebInputEventFactory::keyboardEvent() for details.
  if (wke->text[0]) {
    wke->type = WebKit::WebInputEvent::Char;
    wke->skip_in_browser = true;
    host_view_->ForwardKeyboardEvent(*wke);
  }
}

void GtkIMContextWrapper::ProcessInputMethodResult(const GdkEventKey* event,
                                                   bool filtered) {
  RenderWidgetHost* host = host_view_->GetRenderWidgetHost();
  if (!host)
    return;

  bool committed = false;
  // We do commit before preedit change, so that we can optimize some
  // unnecessary preedit changes.
  if (commit_text_.length()) {
    if (filtered && NeedCommitByForwardingCharEvent()) {
      // Send a Char event when we input a composed character without IMEs
      // so that this event is to be dispatched to onkeypress() handlers,
      // autofill, etc.
      // Only commit text generated by a filtered key down event can be sent
      // as a Char event, because a unfiltered key down event will probably
      // generate another Char event.
      // TODO(james.su@gmail.com): Is it necessary to support non BMP chars
      // here?
      NativeWebKeyboardEvent char_event(commit_text_[0],
                                        event->state,
                                        base::Time::Now().ToDoubleT());
      char_event.skip_in_browser = true;
      host_view_->ForwardKeyboardEvent(char_event);
    } else {
      committed = true;
      // Send an IME event.
      // Unlike a Char event, an IME event is NOT dispatched to onkeypress()
      // handlers or autofill.
      host->ImeConfirmComposition(commit_text_);
      // Set this flag to false, as this composition session has been
      // finished.
      is_composing_text_ = false;
    }
  }

  // Send preedit text only if it's changed.
  // If a text has been committed, then we don't need to send the empty
  // preedit text again.
  if (is_preedit_changed_) {
    if (preedit_text_.length()) {
      // Another composition session has been started.
      is_composing_text_ = true;
      host->ImeSetComposition(preedit_text_, preedit_underlines_,
                              preedit_selection_start_, preedit_selection_end_);
    } else if (!committed) {
      host->ImeCancelComposition();
    }
  }
}

void GtkIMContextWrapper::ConfirmComposition() {
  if (!is_enabled_)
    return;

  DCHECK(!is_in_key_event_handler_);

  if (is_composing_text_) {
    if (host_view_->GetRenderWidgetHost())
      host_view_->GetRenderWidgetHost()->ImeConfirmComposition();

    // Reset the input method.
    CancelComposition();
  }
}

void GtkIMContextWrapper::HandleCommit(const string16& text) {
  if (suppress_next_commit_) {
    suppress_next_commit_ = false;
    return;
  }

  // Append the text to the buffer, because commit signal might be fired
  // multiple times when processing a key event.
  commit_text_.append(text);
  // Nothing needs to do, if it's currently in ProcessKeyEvent()
  // handler, which will send commit text to webkit later. Otherwise,
  // we need send it here.
  // It's possible that commit signal is fired without a key event, for
  // example when user input via a voice or handwriting recognition software.
  // In this case, the text must be committed directly.
  if (!is_in_key_event_handler_ && host_view_->GetRenderWidgetHost()) {
    // Workaround http://crbug.com/45478 by sending fake key down/up events.
    SendFakeCompositionKeyEvent(WebKit::WebInputEvent::RawKeyDown);
    host_view_->GetRenderWidgetHost()->ImeConfirmComposition(text);
    SendFakeCompositionKeyEvent(WebKit::WebInputEvent::KeyUp);
  }
}

void GtkIMContextWrapper::HandlePreeditStart() {
  // Ignore preedit related signals triggered by CancelComposition() method.
  if (suppress_next_commit_)
    return;
  is_composing_text_ = true;
}

void GtkIMContextWrapper::HandlePreeditChanged(const gchar* text,
                                               PangoAttrList* attrs,
                                               int cursor_position) {
  // Ignore preedit related signals triggered by CancelComposition() method.
  if (suppress_next_commit_)
    return;

  // Don't set is_preedit_changed_ to false if there is no change, because
  // this handler might be called multiple times with the same data.
  is_preedit_changed_ = true;
  preedit_text_.clear();
  preedit_underlines_.clear();
  preedit_selection_start_ = 0;
  preedit_selection_end_ = 0;

  ExtractCompositionInfo(text, attrs, cursor_position, &preedit_text_,
                         &preedit_underlines_, &preedit_selection_start_,
                         &preedit_selection_end_);

  // In case we are using a buggy input method which doesn't fire
  // "preedit_start" signal.
  if (preedit_text_.length())
    is_composing_text_ = true;

  // Nothing needs to do, if it's currently in ProcessKeyEvent()
  // handler, which will send preedit text to webkit later.
  // Otherwise, we need send it here if it's been changed.
  if (!is_in_key_event_handler_ && is_composing_text_ &&
      host_view_->GetRenderWidgetHost()) {
    // Workaround http://crbug.com/45478 by sending fake key down/up events.
    SendFakeCompositionKeyEvent(WebKit::WebInputEvent::RawKeyDown);
    host_view_->GetRenderWidgetHost()->ImeSetComposition(
        preedit_text_, preedit_underlines_, preedit_selection_start_,
        preedit_selection_end_);
    SendFakeCompositionKeyEvent(WebKit::WebInputEvent::KeyUp);
  }
}

void GtkIMContextWrapper::HandlePreeditEnd() {
  if (preedit_text_.length()) {
    // The composition session has been finished.
    preedit_text_.clear();
    preedit_underlines_.clear();
    is_preedit_changed_ = true;

    // If there is still a preedit text when firing "preedit-end" signal,
    // we need inform webkit to clear it.
    // It's only necessary when it's not in ProcessKeyEvent ().
    if (!is_in_key_event_handler_ && host_view_->GetRenderWidgetHost())
      host_view_->GetRenderWidgetHost()->ImeCancelComposition();
  }

  // Don't set is_composing_text_ to false here, because "preedit_end"
  // signal may be fired before "commit" signal.
}

void GtkIMContextWrapper::HandleHostViewRealize(GtkWidget* widget) {
  // We should only set im context's client window once, because when setting
  // client window.im context may destroy and recreate its internal states and
  // objects.
  if (widget->window) {
    gtk_im_context_set_client_window(context_, widget->window);
    gtk_im_context_set_client_window(context_simple_, widget->window);
  }
}

void GtkIMContextWrapper::HandleHostViewUnrealize() {
  gtk_im_context_set_client_window(context_, NULL);
  gtk_im_context_set_client_window(context_simple_, NULL);
}

void GtkIMContextWrapper::SendFakeCompositionKeyEvent(
    WebKit::WebInputEvent::Type type) {
  NativeWebKeyboardEvent fake_event;
  fake_event.windowsKeyCode = kCompositionEventKeyCode;
  fake_event.skip_in_browser = true;
  fake_event.type = type;
  host_view_->ForwardKeyboardEvent(fake_event);
}

void GtkIMContextWrapper::HandleCommitThunk(
    GtkIMContext* context, gchar* text, GtkIMContextWrapper* self) {
  self->HandleCommit(UTF8ToUTF16(text));
}

void GtkIMContextWrapper::HandlePreeditStartThunk(
    GtkIMContext* context, GtkIMContextWrapper* self) {
  self->HandlePreeditStart();
}

void GtkIMContextWrapper::HandlePreeditChangedThunk(
    GtkIMContext* context, GtkIMContextWrapper* self) {
  gchar* text = NULL;
  PangoAttrList* attrs = NULL;
  gint cursor_position = 0;
  gtk_im_context_get_preedit_string(context, &text, &attrs, &cursor_position);
  self->HandlePreeditChanged(text, attrs, cursor_position);
  g_free(text);
  pango_attr_list_unref(attrs);
}

void GtkIMContextWrapper::HandlePreeditEndThunk(
    GtkIMContext* context, GtkIMContextWrapper* self) {
  self->HandlePreeditEnd();
}

void GtkIMContextWrapper::HandleHostViewRealizeThunk(
    GtkWidget* widget, GtkIMContextWrapper* self) {
  self->HandleHostViewRealize(widget);
}

void GtkIMContextWrapper::HandleHostViewUnrealizeThunk(
    GtkWidget* widget, GtkIMContextWrapper* self) {
  self->HandleHostViewUnrealize();
}

void GtkIMContextWrapper::ExtractCompositionInfo(
      const gchar* utf8_text,
      PangoAttrList* attrs,
      int cursor_position,
      string16* utf16_text,
      std::vector<WebKit::WebCompositionUnderline>* underlines,
      int* selection_start,
      int* selection_end) {
  *utf16_text = UTF8ToUTF16(utf8_text);

  if (utf16_text->empty())
    return;

  // Gtk/Pango uses character index for cursor position and byte index for
  // attribute range, but we use char16 offset for them. So we need to do
  // conversion here.
  std::vector<int> char16_offsets;
  int length = static_cast<int>(utf16_text->length());
  for (int offset = 0; offset < length; ++offset) {
    char16_offsets.push_back(offset);
    if (CBU16_IS_SURROGATE((*utf16_text)[offset]))
      ++offset;
  }

  // The text length in Unicode characters.
  int char_length = static_cast<int>(char16_offsets.size());
  // Make sure we can convert the value of |char_length| as well.
  char16_offsets.push_back(length);

  int cursor_offset =
      char16_offsets[std::max(0, std::min(char_length, cursor_position))];

  // TODO(suzhe): due to a bug of webkit, we currently can't use selection range
  // with composition string. See: https://bugs.webkit.org/show_bug.cgi?id=40805
  *selection_start = *selection_end = cursor_offset;

  if (attrs) {
    int utf8_length = strlen(utf8_text);
    PangoAttrIterator* iter = pango_attr_list_get_iterator(attrs);

    // We only care about underline and background attributes and convert
    // background attribute into selection if possible.
    do {
      gint start, end;
      pango_attr_iterator_range(iter, &start, &end);

      start = std::min(start, utf8_length);
      end = std::min(end, utf8_length);
      if (start >= end)
        continue;

      start = g_utf8_pointer_to_offset(utf8_text, utf8_text + start);
      end = g_utf8_pointer_to_offset(utf8_text, utf8_text + end);

      // Double check, in case |utf8_text| is not a valid utf-8 string.
      start = std::min(start, char_length);
      end = std::min(end, char_length);
      if (start >= end)
        continue;

      PangoAttribute* background_attr =
          pango_attr_iterator_get(iter, PANGO_ATTR_BACKGROUND);
      PangoAttribute* underline_attr =
          pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE);

      if (background_attr || underline_attr) {
        // Use a black thin underline by default.
        WebKit::WebCompositionUnderline underline(
            char16_offsets[start], char16_offsets[end], SK_ColorBLACK, false);

        // Always use thick underline for a range with background color, which
        // is usually the selection range.
        if (background_attr)
          underline.thick = true;
        if (underline_attr) {
          int type = reinterpret_cast<PangoAttrInt*>(underline_attr)->value;
          if (type == PANGO_UNDERLINE_DOUBLE)
            underline.thick = true;
          else if (type == PANGO_UNDERLINE_ERROR)
            underline.color = SK_ColorRED;
        }
        underlines->push_back(underline);
      }
    } while (pango_attr_iterator_next(iter));
    pango_attr_iterator_destroy(iter);
  }

  // Use a black thin underline by default.
  if (underlines->empty()) {
    underlines->push_back(
        WebKit::WebCompositionUnderline(0, length, SK_ColorBLACK, false));
  }
}

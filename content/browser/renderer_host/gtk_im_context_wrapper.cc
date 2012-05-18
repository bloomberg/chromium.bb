// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gtk_im_context_wrapper.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_gtk.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "ui/base/gtk/gtk_im_context_util.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/rect.h"

using content::NativeWebKeyboardEvent;
using content::RenderWidgetHostImpl;

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

// ui::CompositionUnderline should be identical to
// WebKit::WebCompositionUnderline, so that we can do reinterpret_cast safely.
// TODO(suzhe): remove it after migrating all code in chrome to use
// ui::CompositionUnderline.
COMPILE_ASSERT(sizeof(ui::CompositionUnderline) ==
               sizeof(WebKit::WebCompositionUnderline),
               ui_CompositionUnderline__WebKit_WebCompositionUnderline_diff);

GtkIMContextWrapper::GtkIMContextWrapper(RenderWidgetHostViewGtk* host_view)
    : host_view_(host_view),
      context_(gtk_im_multicontext_new()),
      context_simple_(gtk_im_context_simple_new()),
      is_focused_(false),
      is_composing_text_(false),
      is_enabled_(false),
      is_in_key_event_handler_(false),
      is_composition_changed_(false),
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
  g_signal_connect(context_, "preedit-start",
                   G_CALLBACK(HandlePreeditStartThunk), this);
  g_signal_connect(context_, "preedit-end",
                   G_CALLBACK(HandlePreeditEndThunk), this);
  g_signal_connect(context_, "preedit-changed",
                   G_CALLBACK(HandlePreeditChangedThunk), this);
  g_signal_connect(context_, "commit",
                   G_CALLBACK(HandleCommitThunk), this);
  g_signal_connect(context_, "retrieve-surrounding",
                   G_CALLBACK(HandleRetrieveSurroundingThunk), this);

  g_signal_connect(context_simple_, "preedit-start",
                   G_CALLBACK(HandlePreeditStartThunk), this);
  g_signal_connect(context_simple_, "preedit-end",
                   G_CALLBACK(HandlePreeditEndThunk), this);
  g_signal_connect(context_simple_, "preedit-changed",
                   G_CALLBACK(HandlePreeditChangedThunk), this);
  g_signal_connect(context_simple_, "commit",
                   G_CALLBACK(HandleCommitThunk), this);
  g_signal_connect(context_simple_, "retrieve-surrounding",
                   G_CALLBACK(HandleRetrieveSurroundingThunk), this);

  GtkWidget* widget = host_view->GetNativeView();
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
  is_composition_changed_ = false;
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

  NativeWebKeyboardEvent wke(reinterpret_cast<GdkEvent*>(event));

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

void GtkIMContextWrapper::UpdateInputMethodState(
    ui::TextInputType type,
    bool can_compose_inline) {
  suppress_next_commit_ = false;

  // The renderer has updated its IME status.
  // Control the GtkIMContext object according to this status.
  if (!context_ || !is_focused_)
    return;

  DCHECK(!is_in_key_event_handler_);

  bool is_enabled = (type != ui::TEXT_INPUT_TYPE_NONE &&
      type != ui::TEXT_INPUT_TYPE_PASSWORD);
  if (is_enabled_ != is_enabled) {
    is_enabled_ = is_enabled;
    if (is_enabled)
      gtk_im_context_focus_in(context_);
    else
      gtk_im_context_focus_out(context_);
  }

  if (is_enabled) {
    // If the focused element supports inline rendering of composition text,
    // we receive and send related events to it. Otherwise, the events related
    // to the updates of composition text are directed to the candidate window.
    gtk_im_context_set_use_preedit(context_, can_compose_inline);
  }
}

void GtkIMContextWrapper::UpdateCaretBounds(
    const gfx::Rect& caret_bounds) {
  if (is_enabled_) {
    // Updates the position of the IME candidate window.
    // The position sent from the renderer is a relative one, so we need to
    // attach the GtkIMContext object to this window before changing the
    // position.
    GdkRectangle cursor_rect(caret_bounds.ToGdkRectangle());
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
  if (host_view_->GetRenderWidgetHost()) {
    RenderWidgetHostImpl::From(
        host_view_->GetRenderWidgetHost())->SetInputMethodActive(true);
  }
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
  if (host_view_->GetRenderWidgetHost()) {
    RenderWidgetHostImpl::From(
        host_view_->GetRenderWidgetHost())->SetInputMethodActive(false);
  }
}

GtkWidget* GtkIMContextWrapper::BuildInputMethodsGtkMenu() {
  GtkWidget* submenu = gtk_menu_new();
  gtk_im_multicontext_append_menuitems(GTK_IM_MULTICONTEXT(context_),
                                       GTK_MENU_SHELL(submenu));
  return submenu;
}

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
  composition_.Clear();
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
  return commit_text_.length() || is_composition_changed_;
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
  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      host_view_->GetRenderWidgetHost());
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
  if (is_composition_changed_) {
    if (composition_.text.length()) {
      // Another composition session has been started.
      is_composing_text_ = true;
      // TODO(suzhe): convert both renderer_host and renderer to use
      // ui::CompositionText.
      const std::vector<WebKit::WebCompositionUnderline>& underlines =
          reinterpret_cast<const std::vector<WebKit::WebCompositionUnderline>&>(
              composition_.underlines);
      host->ImeSetComposition(composition_.text, underlines,
                              composition_.selection.start(),
                              composition_.selection.end());
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
    if (host_view_->GetRenderWidgetHost()) {
      RenderWidgetHostImpl::From(
          host_view_->GetRenderWidgetHost())->ImeConfirmComposition();
    }

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
    RenderWidgetHostImpl::From(
        host_view_->GetRenderWidgetHost())->ImeConfirmComposition(text);
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

  // Don't set is_composition_changed_ to false if there is no change, because
  // this handler might be called multiple times with the same data.
  is_composition_changed_ = true;
  composition_.Clear();

  ui::ExtractCompositionTextFromGtkPreedit(text, attrs, cursor_position,
                                           &composition_);

  // TODO(suzhe): due to a bug of webkit, we currently can't use selection range
  // with composition string. See: https://bugs.webkit.org/show_bug.cgi?id=40805
  composition_.selection = ui::Range(cursor_position);

  // In case we are using a buggy input method which doesn't fire
  // "preedit_start" signal.
  if (composition_.text.length())
    is_composing_text_ = true;

  // Nothing needs to do, if it's currently in ProcessKeyEvent()
  // handler, which will send preedit text to webkit later.
  // Otherwise, we need send it here if it's been changed.
  if (!is_in_key_event_handler_ && is_composing_text_ &&
      host_view_->GetRenderWidgetHost()) {
    // Workaround http://crbug.com/45478 by sending fake key down/up events.
    SendFakeCompositionKeyEvent(WebKit::WebInputEvent::RawKeyDown);
    // TODO(suzhe): convert both renderer_host and renderer to use
    // ui::CompositionText.
    const std::vector<WebKit::WebCompositionUnderline>& underlines =
        reinterpret_cast<const std::vector<WebKit::WebCompositionUnderline>&>(
            composition_.underlines);
    RenderWidgetHostImpl::From(
        host_view_->GetRenderWidgetHost())->ImeSetComposition(
            composition_.text, underlines, composition_.selection.start(),
            composition_.selection.end());
    SendFakeCompositionKeyEvent(WebKit::WebInputEvent::KeyUp);
  }
}

void GtkIMContextWrapper::HandlePreeditEnd() {
  if (composition_.text.length()) {
    // The composition session has been finished.
    composition_.Clear();
    is_composition_changed_ = true;

    // If there is still a preedit text when firing "preedit-end" signal,
    // we need inform webkit to clear it.
    // It's only necessary when it's not in ProcessKeyEvent ().
    if (!is_in_key_event_handler_ && host_view_->GetRenderWidgetHost()) {
      RenderWidgetHostImpl::From(
          host_view_->GetRenderWidgetHost())->ImeCancelComposition();
    }
  }

  // Don't set is_composing_text_ to false here, because "preedit_end"
  // signal may be fired before "commit" signal.
}

gboolean GtkIMContextWrapper::HandleRetrieveSurrounding(GtkIMContext* context) {
  if (!is_enabled_)
    return TRUE;

  std::string text;
  size_t cursor_index = 0;

  if (!is_enabled_ || !host_view_->RetrieveSurrounding(&text, &cursor_index)) {
    gtk_im_context_set_surrounding(context, "", 0, 0);
    return TRUE;
  }

  gtk_im_context_set_surrounding(context, text.c_str(), text.length(),
      cursor_index);

  return TRUE;
}

void GtkIMContextWrapper::HandleHostViewRealize(GtkWidget* widget) {
  // We should only set im context's client window once, because when setting
  // client window.im context may destroy and recreate its internal states and
  // objects.
  GdkWindow* gdk_window = gtk_widget_get_window(widget);
  if (gdk_window) {
    gtk_im_context_set_client_window(context_, gdk_window);
    gtk_im_context_set_client_window(context_simple_, gdk_window);
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

gboolean GtkIMContextWrapper::HandleRetrieveSurroundingThunk(
    GtkIMContext* context, GtkIMContextWrapper* self) {
  return self->HandleRetrieveSurrounding(context);
}

void GtkIMContextWrapper::HandleHostViewRealizeThunk(
    GtkWidget* widget, GtkIMContextWrapper* self) {
  self->HandleHostViewRealize(widget);
}

void GtkIMContextWrapper::HandleHostViewUnrealizeThunk(
    GtkWidget* widget, GtkIMContextWrapper* self) {
  self->HandleHostViewUnrealize();
}

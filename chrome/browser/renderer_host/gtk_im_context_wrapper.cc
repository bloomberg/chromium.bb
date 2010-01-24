// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/gtk_im_context_wrapper.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "grit/generated_resources.h"

GtkIMContextWrapper::GtkIMContextWrapper(RenderWidgetHostViewGtk* host_view)
    : host_view_(host_view),
      context_(gtk_im_multicontext_new()),
      context_simple_(gtk_im_context_simple_new()),
      is_focused_(false),
      is_composing_text_(false),
      is_enabled_(false),
      is_in_key_event_handler_(false),
      preedit_cursor_position_(0),
      is_preedit_changed_(false) {
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

  NativeWebKeyboardEvent wke(event);

  // Send filtered keydown event before sending IME result.
  if (event->type == GDK_KEY_PRESS && filtered)
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
  ProcessInputMethodResult(event, filtered);

  // Send unfiltered keydown and keyup events after sending IME result.
  if (event->type == GDK_KEY_PRESS && !filtered)
    ProcessUnfilteredKeyPressEvent(&wke);
  else if (event->type == GDK_KEY_RELEASE)
    host_view_->ForwardKeyboardEvent(wke);

  // End of key event processing.
  is_in_key_event_handler_ = false;
}

void GtkIMContextWrapper::UpdateStatus(int control,
                                       const gfx::Rect& caret_rect) {
  // The renderer has updated its IME status.
  // Control the GtkIMContext object according to this status.
  if (!context_ || !is_focused_)
    return;

  DCHECK(!is_in_key_event_handler_);

  // TODO(james.su@gmail.com): Following code causes a side effect:
  // When trying to move cursor from one text input box to another while
  // composition text is still not confirmed, following CompleteComposition()
  // calls will prevent the cursor from moving outside the first input box.
  if (control == IME_DISABLE) {
    if (is_enabled_) {
      CompleteComposition();
      gtk_im_context_reset(context_simple_);
      gtk_im_context_focus_out(context_);
      is_enabled_ = false;
    }
  } else {
    // Enable the GtkIMContext object if it's not enabled yet.
    if (!is_enabled_) {
      // Reset context_simple_ to its initial state, in case it's currently
      // in middle of a composition session inside a password box.
      gtk_im_context_reset(context_simple_);
      gtk_im_context_focus_in(context_);
      // It might be true when switching from a password box in middle of a
      // composition session.
      is_composing_text_ = false;
      is_enabled_ = true;
    } else if (control == IME_COMPLETE_COMPOSITION) {
      CompleteComposition();
    }

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

  // Notify the GtkIMContext object of this focus-in event only if IME is
  // enabled by WebKit.
  if (is_enabled_)
    gtk_im_context_focus_in(context_);

  // context_simple_ is always enabled.
  // Actually it doesn't care focus state at all.
  gtk_im_context_focus_in(context_simple_);

  // Enables RenderWidget's IME related events, so that we can be notified
  // when WebKit wants to enable or disable IME.
  host_view_->GetRenderWidgetHost()->ImeSetInputMode(true);
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
    CompleteComposition();
    gtk_im_context_focus_out(context_);
  }

  // To make sure it'll be in correct state when focused in again.
  gtk_im_context_reset(context_simple_);
  gtk_im_context_focus_out(context_simple_);

  // Reset stored IME status.
  is_composing_text_ = false;
  preedit_text_.clear();
  preedit_cursor_position_ = 0;

  // Disable RenderWidget's IME related events to save bandwidth.
  host_view_->GetRenderWidgetHost()->ImeSetInputMode(false);
}

void GtkIMContextWrapper::AppendInputMethodsContextMenu(MenuGtk* menu) {
  std::string label = gtk_util::ConvertAcceleratorsFromWindowsStyle(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_INPUT_METHODS_MENU));
  GtkWidget* menuitem = gtk_menu_item_new_with_mnemonic(label.c_str());
  GtkWidget* submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
  gtk_im_multicontext_append_menuitems(GTK_IM_MULTICONTEXT(context_),
                                       GTK_MENU_SHELL(submenu));
  menu->AppendSeparator();
  menu->AppendMenuItem(IDC_INPUT_METHODS_MENU, menuitem);
}

bool GtkIMContextWrapper::NeedCommitByForwardingCharEvent() {
  // If there is no composition text and has only one character to be
  // committed, then the character will be send to webkit as a Char event
  // instead of a confirmed composition text.
  // It should be fine to handle BMP character only, as non-BMP characters
  // can always be committed as confirmed composition text.
  return !is_composing_text_ && commit_text_.length() == 1;
}

void GtkIMContextWrapper::ProcessFilteredKeyPressEvent(
    NativeWebKeyboardEvent* wke) {
  // Copied from third_party/WebKit/WebCore/page/EventHandler.cpp
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
    // Prevent RenderView::UnhandledKeyboardEvent() from processing it.
    // Otherwise unexpected result may occur. For example if it's a
    // Backspace key event, the browser may go back to previous page.
    if (wke->os_event) {
      wke->os_event->keyval = GDK_VoidSymbol;
      wke->os_event->hardware_keycode = 0;
      wke->os_event->state = 0;
    }
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
    host_view_->ForwardKeyboardEvent(*wke);
  }
}

void GtkIMContextWrapper::ProcessInputMethodResult(const GdkEventKey* event,
                                                   bool filtered) {
  RenderWidgetHost* host = host_view_->GetRenderWidgetHost();
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
      host->ImeSetComposition(preedit_text_, preedit_cursor_position_,
                              -1, -1);
    } else if (!committed) {
      host->ImeCancelComposition();
    }
  }
}

void GtkIMContextWrapper::CompleteComposition() {
  if (!is_enabled_)
    return;

  // If WebKit requires to complete current composition, then we need commit
  // existing preedit text and reset the GtkIMContext object.

  // Backup existing preedit text to avoid it's being cleared when resetting
  // the GtkIMContext object.
  string16 old_preedit_text = preedit_text_;

  // Clear it so that we can know if anything is committed by following
  // line.
  commit_text_.clear();

  // Resetting the GtkIMContext. Input method may commit something at this
  // point. In this case, we shall not commit the preedit text again.
  gtk_im_context_reset(context_);

  // If nothing was committed by above line, then commit stored preedit text
  // to prevent data loss.
  if (old_preedit_text.length() && commit_text_.length() == 0) {
    host_view_->GetRenderWidgetHost()->ImeConfirmComposition(
        old_preedit_text);
  }

  is_composing_text_ = false;
  preedit_text_.clear();
  preedit_cursor_position_ = 0;
}

void GtkIMContextWrapper::HandleCommit(const string16& text) {
  // Append the text to the buffer, because commit signal might be fired
  // multiple times when processing a key event.
  commit_text_.append(text);
  // Nothing needs to do, if it's currently in ProcessKeyEvent()
  // handler, which will send commit text to webkit later. Otherwise,
  // we need send it here.
  // It's possible that commit signal is fired without a key event, for
  // example when user input via a voice or handwriting recognition software.
  // In this case, the text must be committed directly.
  if (!is_in_key_event_handler_) {
    host_view_->GetRenderWidgetHost()->ImeConfirmComposition(text);
  }
}

void GtkIMContextWrapper::HandlePreeditStart() {
  is_composing_text_ = true;
}

void GtkIMContextWrapper::HandlePreeditChanged(const string16& text,
                                               int cursor_position) {
  bool changed = false;
  // If preedit text or cursor position is not changed since last time,
  // then it's not necessary to update it again.
  // Preedit text is always stored, so that we can commit it when webkit
  // requires.
  // Don't set is_preedit_changed_ to false if there is no change, because
  // this handler might be called multiple times with the same data.
  if (cursor_position != preedit_cursor_position_ || text != preedit_text_) {
    preedit_text_ = text;
    preedit_cursor_position_ = cursor_position;
    is_preedit_changed_ = true;
    changed = true;
  }

  // In case we are using a buggy input method which doesn't fire
  // "preedit_start" signal.
  if (text.length())
    is_composing_text_ = true;

  // Nothing needs to do, if it's currently in ProcessKeyEvent()
  // handler, which will send preedit text to webkit later.
  // Otherwise, we need send it here if it's been changed.
  if (!is_in_key_event_handler_ && changed) {
    if (text.length()) {
      host_view_->GetRenderWidgetHost()->ImeSetComposition(
          text, cursor_position, -1, -1);
    } else {
      host_view_->GetRenderWidgetHost()->ImeCancelComposition();
    }
  }
}

void GtkIMContextWrapper::HandlePreeditEnd() {
  bool changed = false;
  if (preedit_text_.length()) {
    // The composition session has been finished.
    preedit_text_.clear();
    preedit_cursor_position_ = 0;
    is_preedit_changed_ = true;
    changed = true;
  }

  // If there is still a preedit text when firing "preedit-end" signal,
  // we need inform webkit to clear it.
  // It's only necessary when it's not in ProcessKeyEvent ().
  if (!is_in_key_event_handler_ && changed) {
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
  gint cursor_position = 0;
  gtk_im_context_get_preedit_string(context, &text, NULL, &cursor_position);
  self->HandlePreeditChanged(UTF8ToUTF16(text), cursor_position);
  g_free(text);
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

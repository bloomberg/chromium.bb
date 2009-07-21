// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <cairo/cairo.h>

#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/x11_util.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "webkit/api/public/gtk/WebInputEventFactory.h"
#include "webkit/glue/webcursor_gtk_data.h"

static const int kMaxWindowWidth = 4000;
static const int kMaxWindowHeight = 4000;

using WebKit::WebInputEventFactory;

// This class is a conveience wrapper for GtkIMContext.
class RenderWidgetHostViewGtkIMContext {
 public:
  explicit RenderWidgetHostViewGtkIMContext(RenderWidgetHostViewGtk* host_view)
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
  }

  ~RenderWidgetHostViewGtkIMContext() {
    if (context_)
      g_object_unref(context_);
    if (context_simple_)
      g_object_unref(context_simple_);
  }

  void ProcessKeyEvent(GdkEventKey* event) {
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
      host_view_->GetRenderWidgetHost()->ForwardKeyboardEvent(wke);

    // End of key event processing.
    is_in_key_event_handler_ = false;
  }

  void UpdateStatus(int control, const gfx::Rect& caret_rect) {
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

  void OnFocusIn() {
    if (is_focused_)
      return;

    // Tracks the focused state so that we can give focus to the
    // GtkIMContext object correctly later when IME is enabled by WebKit.
    is_focused_ = true;

    // We should call gtk_im_context_set_client_window() only when this window
    // gain (or release) the window focus because an immodule may reset its
    // internal status when processing this function.
    gtk_im_context_set_client_window(context_,
                                     host_view_->native_view()->window);

    // Notify the GtkIMContext object of this focus-in event only if IME is
    // enabled by WebKit.
    if (is_enabled_)
      gtk_im_context_focus_in(context_);

    // Actually current GtkIMContextSimple implementation doesn't care about
    // client window. This line is just for safe.
    gtk_im_context_set_client_window(context_simple_,
                                     host_view_->native_view()->window);

    // context_simple_ is always enabled.
    // Actually it doesn't care focus state at all.
    gtk_im_context_focus_in(context_simple_);

    // Enables RenderWidget's IME related events, so that we can be notified
    // when WebKit wants to enable or disable IME.
    host_view_->GetRenderWidgetHost()->ImeSetInputMode(true);
  }

  void OnFocusOut() {
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

    // Detach this GtkIMContext object from this window.
    gtk_im_context_set_client_window(context_, NULL);

    // To make sure it'll be in correct state when focused in again.
    gtk_im_context_reset(context_simple_);
    gtk_im_context_focus_out(context_simple_);
    gtk_im_context_set_client_window(context_simple_, NULL);

    // Reset stored IME status.
    is_composing_text_ = false;
    preedit_text_.clear();
    preedit_cursor_position_ = 0;

    // Disable RenderWidget's IME related events to save bandwidth.
    host_view_->GetRenderWidgetHost()->ImeSetInputMode(false);
  }

 private:
  // Check if a text needs commit by forwarding a char event instead of
  // by confirming as a composition text.
  bool NeedCommitByForwardingCharEvent() {
    // If there is no composition text and has only one character to be
    // committed, then the character will be send to webkit as a Char event
    // instead of a confirmed composition text.
    // It should be fine to handle BMP character only, as non-BMP characters
    // can always be committed as confirmed composition text.
    return !is_composing_text_ && commit_text_.length() == 1;
  }

  void ProcessFilteredKeyPressEvent(NativeWebKeyboardEvent* wke) {
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
      // Prevent RenderView::UnhandledKeyboardEvent() from processing it.
      // Otherwise unexpected result may occur. For example if it's a
      // Backspace key event, the browser may go back to previous page.
      if (wke->os_event) {
        wke->os_event->keyval = GDK_VoidSymbol;
        wke->os_event->state = 0;
      }
    }
    host_view_->GetRenderWidgetHost()->ForwardKeyboardEvent(*wke);
  }

  void ProcessUnfilteredKeyPressEvent(NativeWebKeyboardEvent* wke) {
    RenderWidgetHost* host = host_view_->GetRenderWidgetHost();

    // Send keydown event as it, because it's not filtered by IME.
    host->ForwardKeyboardEvent(*wke);

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
      host->ForwardKeyboardEvent(*wke);
    }
  }

  // Processes result returned from input method after filtering a key event.
  // |filtered| indicates if the key event was filtered by the input method.
  void ProcessInputMethodResult(const GdkEventKey* event, bool filtered) {
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
        host->ForwardKeyboardEvent(char_event);
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

  void CompleteComposition() {
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

  // Real code of "commit" signal handler.
  void HandleCommit(const string16& text) {
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

  // Real code of "preedit-start" signal handler.
  void HandlePreeditStart() {
    is_composing_text_ = true;
  }

  // Real code of "preedit-changed" signal handler.
  void HandlePreeditChanged(const string16& text, int cursor_position) {
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

  // Real code of "preedit-end" signal handler.
  void HandlePreeditEnd() {
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

 private:
  // Signal handlers of GtkIMContext object.
  static void HandleCommitThunk(GtkIMContext* context, gchar* text,
                                RenderWidgetHostViewGtkIMContext* self) {
    self->HandleCommit(UTF8ToUTF16(text));
  }

  static void HandlePreeditStartThunk(GtkIMContext* context,
                                      RenderWidgetHostViewGtkIMContext* self) {
    self->HandlePreeditStart();
  }

  static void HandlePreeditChangedThunk(
      GtkIMContext* context, RenderWidgetHostViewGtkIMContext* self) {
    gchar* text = NULL;
    gint cursor_position = 0;
    gtk_im_context_get_preedit_string(context, &text, NULL, &cursor_position);
    self->HandlePreeditChanged(UTF8ToUTF16(text), cursor_position);
    g_free(text);
  }

  static void HandlePreeditEndThunk(GtkIMContext* context,
                                    RenderWidgetHostViewGtkIMContext* self) {
    self->HandlePreeditEnd();
  }

 private:
  // The parent object.
  RenderWidgetHostViewGtk* host_view_;

  // The GtkIMContext object.
  // In terms of the DOM event specification Appendix A
  //   <http://www.w3.org/TR/DOM-Level-3-Events/keyset.html>,
  // GTK uses a GtkIMContext object for the following two purposes:
  //  1. Composing Latin characters (A.1.2), and;
  //  2. Composing CJK characters with an IME (A.1.3).
  // Many JavaScript pages assume composed Latin characters are dispatched to
  // their onkeypress() handlers but not dispatched CJK characters composed
  // with an IME. To emulate this behavior, we should monitor the status of
  // this GtkIMContext object and prevent sending Char events when a
  // GtkIMContext object sends a "commit" signal with the CJK characters
  // composed by an IME.
  GtkIMContext* context_;

  // A GtkIMContextSimple object, for supporting dead/compose keys when input
  // method is disabled, eg. in password input box.
  GtkIMContext* context_simple_;

  // Whether or not this widget is focused.
  bool is_focused_;

  // Whether or not the above GtkIMContext is composing a text with an IME.
  // This flag is used in "commit" signal handler of the GtkIMContext object,
  // which determines how to submit the result text to WebKit according to this
  // flag.
  // If this flag is true or there are more than one characters in the result,
  // then the result text will be committed to WebKit as a confirmed
  // composition. Otherwise, it'll be forwarded as a key event.
  //
  // The GtkIMContext object sends a "preedit_start" before it starts composing
  // a text and a "preedit_end" signal after it finishes composing it.
  // "preedit_start" signal is monitored to turn it on.
  // We don't monitor "preedit_end" signal to turn it off, because an input
  // method may fire "preedit_end" signal before "commit" signal.
  // A buggy input method may not fire "preedit_start" and/or "preedit_end"
  // at all, so this flag will also be set to true when "preedit_changed" signal
  // is fired with non-empty preedit text.
  bool is_composing_text_;

  // Whether or not the IME is enabled.
  // This flag is actually controlled by RenderWidget.
  // It shall be set to false when an ImeUpdateStatus message with control ==
  // IME_DISABLE is received, and shall be set to true if control ==
  // IME_COMPLETE_COMPOSITION or IME_MOVE_WINDOWS.
  // When this flag is false, keyboard events shall be dispatched directly
  // instead of sending to context_.
  bool is_enabled_;

  // Whether or not it's currently running inside key event handler.
  // If it's true, then preedit-changed and commit handler will backup the
  // preedit or commit text instead of sending them down to webkit.
  // key event handler will send them later.
  bool is_in_key_event_handler_;

  // Stores a copy of the most recent preedit text retrieved from context_.
  // When an ImeUpdateStatus message with control == IME_COMPLETE_COMPOSITION
  // is received, this stored preedit text (if not empty) shall be committed,
  // and context_ shall be reset.
  string16 preedit_text_;

  // Stores the cursor position in the stored preedit text.
  int preedit_cursor_position_;

  // Whether or not the preedit has been changed since last key event.
  bool is_preedit_changed_;

  // Stores a copy of the most recent commit text received by commit signal
  // handler.
  string16 commit_text_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGtkIMContext);
};

// This class is a simple convenience wrapper for Gtk functions. It has only
// static methods.
class RenderWidgetHostViewGtkWidget {
 public:
  static GtkWidget* CreateNewWidget(RenderWidgetHostViewGtk* host_view) {
    GtkWidget* widget = gtk_fixed_new();
    gtk_fixed_set_has_window(GTK_FIXED(widget), TRUE);
    gtk_widget_set_double_buffered(widget, FALSE);
    gtk_widget_set_redraw_on_allocate(widget, FALSE);
#if defined(NDEBUG)
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &gfx::kGdkWhite);
#else
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &gfx::kGdkGreen);
#endif

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                  GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK |
                                  GDK_KEY_PRESS_MASK |
                                  GDK_KEY_RELEASE_MASK);
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

    g_signal_connect(widget, "size-allocate",
                     G_CALLBACK(SizeAllocate), host_view);
    g_signal_connect(widget, "expose-event",
                     G_CALLBACK(ExposeEvent), host_view);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "key-release-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "focus-in-event",
                     G_CALLBACK(OnFocusIn), host_view);
    g_signal_connect(widget, "focus-out-event",
                     G_CALLBACK(OnFocusOut), host_view);
    g_signal_connect(widget, "grab-notify",
                     G_CALLBACK(OnGrabNotify), host_view);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "button-release-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(MouseMoveEvent), host_view);
    // Connect after so that we are called after the handler installed by the
    // TabContentsView which handles zoom events.
    g_signal_connect_after(widget, "scroll-event",
                           G_CALLBACK(MouseScrollEvent), host_view);

    // Create GtkIMContext wrapper object.
    host_view->im_context_.reset(
        new RenderWidgetHostViewGtkIMContext(host_view));

    return widget;
  }

 private:
  static gboolean SizeAllocate(GtkWidget* widget, GtkAllocation* allocation,
                               RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->WasResized();
    return FALSE;
  }

  static gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                              RenderWidgetHostViewGtk* host_view) {
    const gfx::Rect damage_rect(expose->area);
    host_view->Paint(damage_rect);
    return FALSE;
  }

  static gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                       RenderWidgetHostViewGtk* host_view) {
    if (host_view->parent_ && host_view->activatable() &&
        GDK_Escape == event->keyval) {
      // Force popups to close on Esc just in case the renderer is hung.  This
      // allows us to release our keyboard grab.
      host_view->host_->Shutdown();
    } else {
      // Send key event to input method.
      host_view->im_context_->ProcessKeyEvent(event);
    }

    // We return TRUE because we did handle the event. If it turns out webkit
    // can't handle the event, we'll deal with it in
    // RenderView::UnhandledKeyboardEvent().
    return TRUE;
  }

  // WARNING: OnGrabNotify relies on the fact this function doesn't try to
  // dereference |focus|.
  static gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* focus,
                            RenderWidgetHostViewGtk* host_view) {
    int x, y;
    gtk_widget_get_pointer(widget, &x, &y);
    // http://crbug.com/13389
    // If the cursor is in the render view, fake a mouse move event so that
    // webkit updates its state. Otherwise webkit might think the cursor is
    // somewhere it's not.
    if (x >= 0 && y >= 0 && x < widget->allocation.width &&
        y < widget->allocation.height) {
      WebKit::WebMouseEvent fake_event;
      fake_event.timeStampSeconds = base::Time::Now().ToDoubleT();
      fake_event.modifiers = 0;
      fake_event.windowX = fake_event.x = x;
      fake_event.windowY = fake_event.y = y;
      gdk_window_get_origin(widget->window, &x, &y);
      fake_event.globalX = fake_event.x + x;
      fake_event.globalY = fake_event.y + y;
      fake_event.type = WebKit::WebInputEvent::MouseMove;
      fake_event.button = WebKit::WebMouseEvent::ButtonNone;
      host_view->GetRenderWidgetHost()->ForwardMouseEvent(fake_event);
    }

    host_view->ShowCurrentCursor();
    host_view->GetRenderWidgetHost()->Focus();

    // The only way to enable a GtkIMContext object is to call its focus in
    // handler.
    host_view->im_context_->OnFocusIn();

    return FALSE;
  }

  // WARNING: OnGrabNotify relies on the fact this function doesn't try to
  // dereference |focus|.
  static gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* focus,
                             RenderWidgetHostViewGtk* host_view) {
    // Whenever we lose focus, set the cursor back to that of our parent window,
    // which should be the default arrow.
    gdk_window_set_cursor(widget->window, NULL);
    // If we are showing a context menu, maintain the illusion that webkit has
    // focus.
    if (!host_view->is_showing_context_menu_)
      host_view->GetRenderWidgetHost()->Blur();

    // Disable the GtkIMContext object.
    host_view->im_context_->OnFocusOut();

    return FALSE;
  }

  // Called when we are shadowed or unshadowed by a keyboard grab (which will
  // occur for activatable popups, such as dropdown menus). Popup windows do not
  // take focus, so we never get a focus out or focus in event when they are
  // shown, and must rely on this signal instead.
  static void OnGrabNotify(GtkWidget* widget, gboolean was_grabbed,
                           RenderWidgetHostViewGtk* host_view) {
    if (was_grabbed) {
      if (host_view->was_focused_before_grab_)
        OnFocusIn(widget, NULL, host_view);
    } else {
      host_view->was_focused_before_grab_ = host_view->HasFocus();
      if (host_view->was_focused_before_grab_)
        OnFocusOut(widget, NULL, host_view);
    }
  }

  static gboolean ButtonPressReleaseEvent(
      GtkWidget* widget, GdkEventButton* event,
      RenderWidgetHostViewGtk* host_view) {
    if (!(event->button == 1 || event->button == 2 || event->button == 3))
      return FALSE;  // We do not forward any other buttons to the renderer.

    // We want to translate the coordinates of events that do not originate
    // from this widget to be relative to the top left of the widget.
    GtkWidget* event_widget = gtk_get_event_widget(
        reinterpret_cast<GdkEvent*>(event));
    if (event_widget != widget) {
      int x = 0;
      int y = 0;
      gtk_widget_get_pointer(widget, &x, &y);
      // If the mouse event happens outside our popup, force the popup to
      // close.  We do this so a hung renderer doesn't prevent us from
      // releasing the x pointer grab.
      bool click_in_popup = x >= 0 && y >= 0 && x < widget->allocation.width &&
          y < widget->allocation.height;
      // Only Shutdown on mouse downs. Mouse ups can occur outside the render
      // view if the user drags for DnD or while using the scrollbar on a select
      // dropdown. Don't shutdown if we are not a popup.
      if (event->type != GDK_BUTTON_RELEASE && host_view->parent_ &&
          !host_view->is_popup_first_mouse_release_ && !click_in_popup) {
        host_view->host_->Shutdown();
        return FALSE;
      }
      event->x = x;
      event->y = y;
    }
    host_view->is_popup_first_mouse_release_ = false;
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(
        WebInputEventFactory::mouseEvent(event));

    // TODO(evanm): why is this necessary here but not in test shell?
    // This logic is the same as GtkButton.
    if (event->type == GDK_BUTTON_PRESS && !GTK_WIDGET_HAS_FOCUS(widget))
      gtk_widget_grab_focus(widget);

    // Although we did handle the mouse event, we need to let other handlers
    // run (in particular the one installed by TabContentsViewGtk).
    return FALSE;
  }

  static gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                                 RenderWidgetHostViewGtk* host_view) {
    // We want to translate the coordinates of events that do not originate
    // from this widget to be relative to the top left of the widget.
    GtkWidget* event_widget = gtk_get_event_widget(
        reinterpret_cast<GdkEvent*>(event));
    if (event_widget != widget) {
      int x = 0;
      int y = 0;
      gtk_widget_get_pointer(widget, &x, &y);
      event->x = x;
      event->y = y;
    }
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(
        WebInputEventFactory::mouseEvent(event));
    return FALSE;
  }

  static gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                                   RenderWidgetHostViewGtk* host_view) {
    // If the user is holding shift, translate it into a horizontal scroll. We
    // don't care what other modifiers the user may be holding (zooming is
    // handled at the TabContentsView level).
    if (event->state & GDK_SHIFT_MASK) {
      if (event->direction == GDK_SCROLL_UP)
        event->direction = GDK_SCROLL_LEFT;
      else if (event->direction == GDK_SCROLL_DOWN)
        event->direction = GDK_SCROLL_RIGHT;
    }

    host_view->GetRenderWidgetHost()->ForwardWheelEvent(
        WebInputEventFactory::mouseWheelEvent(event));
    return FALSE;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWidgetHostViewGtkWidget);
};

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewGtk(widget);
}

RenderWidgetHostViewGtk::RenderWidgetHostViewGtk(RenderWidgetHost* widget_host)
    : host_(widget_host),
      about_to_validate_and_paint_(false),
      is_hidden_(false),
      is_loading_(false),
      is_showing_context_menu_(false),
      parent_host_view_(NULL),
      parent_(NULL),
      is_popup_first_mouse_release_(true),
      was_focused_before_grab_(false) {
  host_->set_view(this);
}

RenderWidgetHostViewGtk::~RenderWidgetHostViewGtk() {
  view_.Destroy();
}

void RenderWidgetHostViewGtk::InitAsChild() {
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  plugin_container_manager_.set_host_widget(view_.get());
  gtk_widget_show(view_.get());
}

void RenderWidgetHostViewGtk::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  parent_host_view_ = parent_host_view;
  parent_ = parent_host_view->GetNativeView();
  GtkWidget* popup = gtk_window_new(GTK_WINDOW_POPUP);
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  plugin_container_manager_.set_host_widget(view_.get());
  gtk_container_add(GTK_CONTAINER(popup), view_.get());

  // If we are not activatable, we don't want to grab keyboard input,
  // and webkit will manage our destruction.
  if (activatable()) {
    // Grab all input for the app. If a click lands outside the bounds of the
    // popup, WebKit will notice and destroy us.
    gtk_grab_add(view_.get());
    // Now grab all of X's input.
    gdk_pointer_grab(
        parent_->window,
        TRUE,  // Only events outside of the window are reported with respect
               // to |parent_->window|.
        static_cast<GdkEventMask>(GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK),
        NULL,
        NULL,
        GDK_CURRENT_TIME);
    // We grab keyboard events too so things like alt+tab are eaten.
    gdk_keyboard_grab(parent_->window, TRUE, GDK_CURRENT_TIME);

    // Our parent widget actually keeps GTK focus within its window, but we have
    // to make the webkit selection box disappear to maintain appearances.
    parent_host_view->Blur();
  }

  gtk_widget_set_size_request(view_.get(),
                              std::min(pos.width(), kMaxWindowWidth),
                              std::min(pos.height(), kMaxWindowHeight));

  gtk_window_set_default_size(GTK_WINDOW(popup), -1, -1);
  // Don't allow the window to be resized. This also forces the window to
  // shrink down to the size of its child contents.
  gtk_window_set_resizable(GTK_WINDOW(popup), FALSE);
  gtk_window_move(GTK_WINDOW(popup), pos.x(), pos.y());
  gtk_widget_show_all(popup);
}

void RenderWidgetHostViewGtk::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  is_hidden_ = false;
  host_->WasRestored();
}

void RenderWidgetHostViewGtk::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  GetRenderWidgetHost()->WasHidden();
}

void RenderWidgetHostViewGtk::SetSize(const gfx::Size& size) {
  // This is called when webkit has sent us a Move message.
  // If we are a popup, we want to handle this.
  // TODO(estade): are there other situations where we want to respect the
  // request?
#if !defined(TOOLKIT_VIEWS)
  if (parent_) {
#else
  // TOOLKIT_VIEWS' resize logic flow matches windows. When the container widget
  // is resized, it calls RWH::WasSized, which sizes this widget using SetSize.
  // TODO(estade): figure out if the logic flow here can be normalized across
  //               platforms
#endif
    gtk_widget_set_size_request(view_.get(),
                                std::min(size.width(), kMaxWindowWidth),
                                std::min(size.height(), kMaxWindowHeight));
#if !defined(TOOLKIT_VIEWS)
  }
#endif
}

gfx::NativeView RenderWidgetHostViewGtk::GetNativeView() {
  return view_.get();
}

void RenderWidgetHostViewGtk::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  for (size_t i = 0; i < plugin_window_moves.size(); ++i) {
    plugin_container_manager_.MovePluginContainer(plugin_window_moves[i]);
  }
}

void RenderWidgetHostViewGtk::Focus() {
  gtk_widget_grab_focus(view_.get());
}

void RenderWidgetHostViewGtk::Blur() {
  // TODO(estade): We should be clearing native focus as well, but I know of no
  // way to do that without focusing another widget.
  // TODO(estade): it doesn't seem like the CanBlur() check should be necessary
  // since we are only called in response to a ViewHost blur message, but this
  // check is made on Windows so I've added it here as well.
  if (host_->CanBlur())
    host_->Blur();
}

bool RenderWidgetHostViewGtk::HasFocus() {
  return gtk_widget_is_focus(view_.get());
}

void RenderWidgetHostViewGtk::Show() {
  gtk_widget_show(view_.get());
}

void RenderWidgetHostViewGtk::Hide() {
  gtk_widget_hide(view_.get());
}

gfx::Rect RenderWidgetHostViewGtk::GetViewBounds() const {
  GtkAllocation* alloc = &view_.get()->allocation;
  return gfx::Rect(alloc->x, alloc->y, alloc->width, alloc->height);
}

void RenderWidgetHostViewGtk::UpdateCursor(const WebCursor& cursor) {
  // Optimize the common case, where the cursor hasn't changed.
  // However, we can switch between different pixmaps, so only on the
  // non-pixmap branch.
  if (current_cursor_.GetCursorType() != GDK_CURSOR_IS_PIXMAP &&
      current_cursor_.GetCursorType() == cursor.GetCursorType())
    return;

  current_cursor_ = cursor;
  ShowCurrentCursor();
}

void RenderWidgetHostViewGtk::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // Only call ShowCurrentCursor() when it will actually change the cursor.
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR)
    ShowCurrentCursor();
}

void RenderWidgetHostViewGtk::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  im_context_->UpdateStatus(control, caret_rect);
}

void RenderWidgetHostViewGtk::DidPaintRect(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  if (about_to_validate_and_paint_)
    invalid_rect_ = invalid_rect_.Union(rect);
  else
    Paint(rect);
}

void RenderWidgetHostViewGtk::DidScrollRect(const gfx::Rect& rect, int dx,
                                            int dy) {
  if (is_hidden_)
    return;

  Paint(rect);
}

void RenderWidgetHostViewGtk::RenderViewGone() {
  Destroy();
  plugin_container_manager_.set_host_widget(NULL);
}

void RenderWidgetHostViewGtk::Destroy() {
  // If |parent_| is non-null, we are a popup and we must disconnect from our
  // parent and destroy the popup window.
  if (parent_) {
    if (activatable()) {
      GdkDisplay *display = gtk_widget_get_display(parent_);
      gdk_display_pointer_ungrab(display, GDK_CURRENT_TIME);
      gdk_display_keyboard_ungrab(display, GDK_CURRENT_TIME);
      parent_host_view_->Focus();
    }
    gtk_widget_destroy(gtk_widget_get_parent(view_.get()));
  }

  // Remove |view_| from all containers now, so nothing else can hold a
  // reference to |view_|'s widget except possibly a gtk signal handler if
  // this code is currently executing within the context of a gtk signal
  // handler.  Note that |view_| is still alive after this call.  It will be
  // deallocated in the destructor.
  // See http://www.crbug.com/11847 for details.
  gtk_widget_destroy(view_.get());

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewGtk::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text.empty()) {
    gtk_widget_set_has_tooltip(view_.get(), FALSE);
  } else {
    gtk_widget_set_tooltip_text(view_.get(), WideToUTF8(tooltip_text).c_str());
  }
}

void RenderWidgetHostViewGtk::SelectionChanged(const std::string& text) {
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_set_text(x_clipboard, text.c_str(), text.length());
}

void RenderWidgetHostViewGtk::PasteFromSelectionClipboard() {
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_request_text(x_clipboard, ReceivedSelectionText, this);
}

void RenderWidgetHostViewGtk::ShowingContextMenu(bool showing) {
  is_showing_context_menu_ = showing;
}

BackingStore* RenderWidgetHostViewGtk::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStore(host_, size,
                          x11_util::GetVisualFromGtkWidget(view_.get()),
                          gtk_widget_get_visual(view_.get())->depth);
}

void RenderWidgetHostViewGtk::Paint(const gfx::Rect& damage_rect) {
  DCHECK(!about_to_validate_and_paint_);

  invalid_rect_ = damage_rect;
  about_to_validate_and_paint_ = true;
  BackingStore* backing_store = host_->GetBackingStore(true);
  // Calling GetBackingStore maybe have changed |invalid_rect_|...
  about_to_validate_and_paint_ = false;

  gfx::Rect paint_rect = gfx::Rect(0, 0, kMaxWindowWidth, kMaxWindowHeight);
  paint_rect = paint_rect.Intersect(invalid_rect_);

  GdkWindow* window = view_.get()->window;
  if (backing_store) {
    // Only render the widget if it is attached to a window; there's a short
    // period where this object isn't attached to a window but hasn't been
    // Destroy()ed yet and it receives paint messages...
    if (window) {
      backing_store->ShowRect(
          paint_rect, x11_util::GetX11WindowFromGtkWidget(view_.get()));
    }
  } else {
    if (window)
      gdk_window_clear(window);
  }
}

void RenderWidgetHostViewGtk::ShowCurrentCursor() {
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!view_.get()->window)
    return;

  GdkCursor* gdk_cursor;
  switch (current_cursor_.GetCursorType()) {
    case GDK_CURSOR_IS_PIXMAP:
      // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
      // that calling gdk_window_set_cursor repeatedly is expensive.  We should
      // avoid it here where possible.
      gdk_cursor = current_cursor_.GetCustomCursor();
      break;

    case GDK_LAST_CURSOR:
      if (is_loading_) {
        // Use MOZ_CURSOR_SPINNING if we are showing the default cursor and
        // the page is loading.
        static const GdkColor fg = { 0, 0, 0, 0 };
        static const GdkColor bg = { 65535, 65535, 65535, 65535 };
        GdkPixmap* source =
            gdk_bitmap_create_from_data(NULL, moz_spinning_bits, 32, 32);
        GdkPixmap* mask =
            gdk_bitmap_create_from_data(NULL, moz_spinning_mask_bits, 32, 32);
        gdk_cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 2, 2);
        g_object_unref(source);
        g_object_unref(mask);
      } else {
        gdk_cursor = NULL;
      }
      break;

    default:
      gdk_cursor = gdk_cursor_new(current_cursor_.GetCursorType());
  }
  gdk_window_set_cursor(view_.get()->window, gdk_cursor);
  // The window now owns the cursor.
  if (gdk_cursor)
    gdk_cursor_unref(gdk_cursor);
}

void RenderWidgetHostViewGtk::ReceivedSelectionText(GtkClipboard* clipboard,
    const gchar* text, gpointer userdata) {
  // If there's nothing to paste (|text| is NULL), do nothing.
  if (!text)
    return;
  RenderWidgetHostViewGtk* host_view =
      reinterpret_cast<RenderWidgetHostViewGtk*>(userdata);
  host_view->host_->Send(new ViewMsg_InsertText(host_view->host_->routing_id(),
                                                UTF8ToUTF16(text)));
}

gfx::PluginWindowHandle RenderWidgetHostViewGtk::CreatePluginContainer(
    base::ProcessId plugin_process_id) {
  gfx::PluginWindowHandle handle =
      plugin_container_manager_.CreatePluginContainer();
  plugin_pid_map_.insert(std::make_pair(plugin_process_id, handle));
  return handle;
}

void RenderWidgetHostViewGtk::DestroyPluginContainer(
    gfx::PluginWindowHandle container) {
  plugin_container_manager_.DestroyPluginContainer(container);

  for (PluginPidMap::iterator i = plugin_pid_map_.begin();
       i != plugin_pid_map_.end(); ++i) {
    if (i->second == container) {
      plugin_pid_map_.erase(i);
      break;
    }
  }
}

void RenderWidgetHostViewGtk::PluginProcessCrashed(base::ProcessId pid) {
  for (PluginPidMap::iterator i = plugin_pid_map_.find(pid);
       i != plugin_pid_map_.end() && i->first == pid; ++i) {
    plugin_container_manager_.DestroyPluginContainer(i->second);
  }
  plugin_pid_map_.erase(pid);
}

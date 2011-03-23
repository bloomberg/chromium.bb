// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_GTK_IM_CONTEXT_WRAPPER_H_
#define CHROME_BROWSER_RENDERER_HOST_GTK_IM_CONTEXT_WRAPPER_H_
#pragma once

#include <gdk/gdk.h>
#include <pango/pango-attributes.h>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextInputType.h"
#include "ui/base/ime/composition_text.h"

namespace gfx {
class Rect;
}

#if !defined(TOOLKIT_VIEWS)
class MenuGtk;
#endif
class RenderWidgetHostViewGtk;
struct NativeWebKeyboardEvent;
typedef struct _GtkIMContext GtkIMContext;
typedef struct _GtkWidget GtkWidget;

// This class is a convenience wrapper for GtkIMContext.
// It creates and manages two GtkIMContext instances, one is GtkIMMulticontext,
// for plain text input box, another is GtkIMContextSimple, for password input
// box.
//
// This class is in charge of dispatching key events to these two GtkIMContext
// instances and handling signals emitted by them. Key events then will be
// forwarded to renderer along with input method results via corresponding host
// view.
//
// This class is used solely by RenderWidgetHostViewGtk.
class GtkIMContextWrapper {
 public:
  explicit GtkIMContextWrapper(RenderWidgetHostViewGtk* host_view);
  ~GtkIMContextWrapper();

  // Processes a gdk key event received by |host_view|.
  void ProcessKeyEvent(GdkEventKey* event);

  void UpdateInputMethodState(WebKit::WebTextInputType type,
                              const gfx::Rect& caret_rect);
  void OnFocusIn();
  void OnFocusOut();

#if !defined(TOOLKIT_VIEWS)
  // Not defined for views because the views context menu doesn't
  // implement input methods yet.
  void AppendInputMethodsContextMenu(MenuGtk* menu);
#endif

  void CancelComposition();

  void ConfirmComposition();

 private:
  // Check if a text needs commit by forwarding a char event instead of
  // by confirming as a composition text.
  bool NeedCommitByForwardingCharEvent() const;

  // Check if the input method returned any result, eg. preedit and commit text.
  bool HasInputMethodResult() const;

  void ProcessFilteredKeyPressEvent(NativeWebKeyboardEvent* wke);
  void ProcessUnfilteredKeyPressEvent(NativeWebKeyboardEvent* wke);

  // Processes result returned from input method after filtering a key event.
  // |filtered| indicates if the key event was filtered by the input method.
  void ProcessInputMethodResult(const GdkEventKey* event, bool filtered);

  // Real code of "commit" signal handler.
  void HandleCommit(const string16& text);

  // Real code of "preedit-start" signal handler.
  void HandlePreeditStart();

  // Real code of "preedit-changed" signal handler.
  void HandlePreeditChanged(const gchar* text,
                            PangoAttrList* attrs,
                            int cursor_position);

  // Real code of "preedit-end" signal handler.
  void HandlePreeditEnd();

  // Real code of "realize" signal handler, used for setting im context's client
  // window.
  void HandleHostViewRealize(GtkWidget* widget);

  // Real code of "unrealize" signal handler, used for unsetting im context's
  // client window.
  void HandleHostViewUnrealize();

  // Sends a fake composition key event with specified event type. A composition
  // key event is a key event with special key code 229.
  void SendFakeCompositionKeyEvent(WebKit::WebInputEvent::Type type);

  // Signal handlers of GtkIMContext object.
  static void HandleCommitThunk(GtkIMContext* context, gchar* text,
                                GtkIMContextWrapper* self);
  static void HandlePreeditStartThunk(GtkIMContext* context,
                                      GtkIMContextWrapper* self);
  static void HandlePreeditChangedThunk(GtkIMContext* context,
                                        GtkIMContextWrapper* self);
  static void HandlePreeditEndThunk(GtkIMContext* context,
                                    GtkIMContextWrapper* self);

  // Signal handlers connecting to |host_view_|'s native view widget.
  static void HandleHostViewRealizeThunk(GtkWidget* widget,
                                         GtkIMContextWrapper* self);
  static void HandleHostViewUnrealizeThunk(GtkWidget* widget,
                                           GtkIMContextWrapper* self);

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
  bool is_enabled_;

  // Whether or not it's currently running inside key event handler.
  // If it's true, then preedit-changed and commit handler will backup the
  // preedit or commit text instead of sending them down to webkit.
  // key event handler will send them later.
  bool is_in_key_event_handler_;

  // The most recent composition text information retrieved from context_;
  ui::CompositionText composition_;

  // Whether or not the composition has been changed since last key event.
  bool is_composition_changed_;

  // Stores a copy of the most recent commit text received by commit signal
  // handler.
  string16 commit_text_;

  // If it's true then the next "commit" signal will be suppressed.
  // It's only used to workaround http://crbug.com/50485.
  // TODO(suzhe): Remove it after input methods get fixed.
  bool suppress_next_commit_;

  // Information of the last key event, for working around
  // http://crosbug.com/6582
  int last_key_code_;
  bool last_key_was_up_;
  bool last_key_filtered_no_result_;

  DISALLOW_COPY_AND_ASSIGN(GtkIMContextWrapper);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_GTK_IM_CONTEXT_WRAPPER_H_

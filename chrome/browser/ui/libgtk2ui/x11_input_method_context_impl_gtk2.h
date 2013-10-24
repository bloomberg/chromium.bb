// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_X11_INPUT_METHOD_CONTEXT_IMPL_GTK2_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_X11_INPUT_METHOD_CONTEXT_IMPL_GTK2_H_

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/gfx/rect.h"

typedef struct _GtkIMContext GtkIMContext;

namespace libgtk2ui {

// An implementation of LinuxInputMethodContext which is based on X11 event loop
// and uses GtkIMContext(gtk-immodule) as a bridge from/to underlying IMEs.
class X11InputMethodContextImplGtk2 : public ui::LinuxInputMethodContext {
 public:
  explicit X11InputMethodContextImplGtk2(
      ui::LinuxInputMethodContextDelegate* delegate);
  virtual ~X11InputMethodContextImplGtk2();

  // Overriden from ui::LinuxInputMethodContext
  virtual bool DispatchKeyEvent(const base::NativeEvent& native_key_event)
      OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() const OVERRIDE;
  virtual void OnTextInputTypeChanged(ui::TextInputType text_input_type)
      OVERRIDE;
  virtual void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) OVERRIDE;

 private:
  // Returns true if the hardware |keycode| is assigned to a modifier key.
  bool IsKeycodeModifierKey(unsigned int keycode) const;

  // GtkIMContext event handlers.  They are shared among |gtk_context_simple_|
  // and |gtk_multicontext_|.
  CHROMEG_CALLBACK_1(X11InputMethodContextImplGtk2, void, OnCommit,
                     GtkIMContext*, gchar*);
  CHROMEG_CALLBACK_0(X11InputMethodContextImplGtk2, void, OnPreeditChanged,
                     GtkIMContext*);
  CHROMEG_CALLBACK_0(X11InputMethodContextImplGtk2, void, OnPreeditEnd,
                     GtkIMContext*);
  CHROMEG_CALLBACK_0(X11InputMethodContextImplGtk2, void, OnPreeditStart,
                     GtkIMContext*);

  // A set of callback functions.  Must not be NULL.
  ui::LinuxInputMethodContextDelegate* delegate_;

  // IME's input context used for TEXT_INPUT_TYPE_NONE and
  // TEXT_INPUT_TYPE_PASSWORD.
  GtkIMContext* gtk_context_simple_;
  // IME's input context used for the other text input types.
  GtkIMContext* gtk_multicontext_;

  // An alias to |gtk_context_simple_| or |gtk_multicontext_| depending on the
  // text input type.  Can be NULL when it's not focused.
  GtkIMContext* gtk_context_;

  // Last known caret bounds relative to the screen coordinates.
  gfx::Rect last_caret_bounds_;

  // A set of hardware keycodes of modifier keys.
  base::hash_set<unsigned int> modifier_keycodes_;

  // The helper class to trap GTK+'s "commit" signal for direct input key
  // events.
  //
  // gtk_im_context_filter_keypress() emits "commit" signal in order to insert
  // a character which is not actually processed by a IME.  This behavior seems,
  // in Javascript world, that a keydown event with keycode = VKEY_PROCESSKEY
  // (= 229) is fired.  So we have to trap such "commit" signal for direct input
  // key events.  This class helps to trap such events.
  class GtkCommitSignalTrap {
   public:
    GtkCommitSignalTrap();

    // Enables the trap which monitors a direct input key event of |keyval|.
    void StartTrap(guint keyval);

    // Disables the trap.
    void StopTrap();

    // Checks if the committed |text| has come from a direct input key event,
    // and returns true in that case.  Once it's trapped, IsSignalCaught()
    // returns true.
    // Must be called at most once between StartTrap() and StopTrap().
    bool Trap(const base::string16& text);

    // Returns true if a direct input key event is detected.
    bool IsSignalCaught() const { return is_signal_caught_; }

   private:
    bool is_trap_enabled_;
    guint gdk_event_key_keyval_;
    bool is_signal_caught_;

    DISALLOW_COPY_AND_ASSIGN(GtkCommitSignalTrap);
  };

  GtkCommitSignalTrap commit_signal_trap_;

  FRIEND_TEST_ALL_PREFIXES(X11InputMethodContextImplGtk2FriendTest,
                           GtkCommitSignalTrap);

  DISALLOW_COPY_AND_ASSIGN(X11InputMethodContextImplGtk2);
};

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_X11_INPUT_METHOD_CONTEXT_IMPL_GTK2_H_

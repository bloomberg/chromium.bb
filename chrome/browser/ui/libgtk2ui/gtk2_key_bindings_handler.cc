// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gtk2_key_bindings_handler.h"

#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"

using ui::TextEditCommandAuraLinux;

// TODO(erg): Rewrite the old gtk_key_bindings_handler_unittest.cc and get them
// in a state that links. This code was adapted from the content layer GTK
// code, which had some simple unit tests. However, the changes in the public
// interface basically meant the tests need to be rewritten; this imposes weird
// linking requirements regarding GTK+ as we don't have a libgtk2ui_unittests
// yet. http://crbug.com/358297.

namespace libgtk2ui {

Gtk2KeyBindingsHandler::Gtk2KeyBindingsHandler()
    : fake_window_(gtk_offscreen_window_new()),
      handler_(CreateNewHandler()),
      has_xkb_(false) {
  gtk_container_add(GTK_CONTAINER(fake_window_), handler_.get());

  int opcode, event, error;
  int major = XkbMajorVersion;
  int minor = XkbMinorVersion;
  has_xkb_ = XkbQueryExtension(gfx::GetXDisplay(), &opcode, &event, &error,
                               &major, &minor);
}

Gtk2KeyBindingsHandler::~Gtk2KeyBindingsHandler() {
  handler_.Destroy();
  gtk_widget_destroy(fake_window_);
}

bool Gtk2KeyBindingsHandler::MatchEvent(
    const ui::Event& event,
    std::vector<TextEditCommandAuraLinux>* edit_commands) {
  CHECK(event.IsKeyEvent());

  const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
  if (key_event.is_char() || !key_event.native_event())
    return false;

  GdkEventKey gdk_event;
  BuildGdkEventKeyFromXEvent(key_event.native_event(), &gdk_event);

  edit_commands_.clear();
  // If this key event matches a predefined key binding, corresponding signal
  // will be emitted.
  gtk_bindings_activate_event(GTK_OBJECT(handler_.get()), &gdk_event);

  bool matched = !edit_commands_.empty();
  if (edit_commands)
    edit_commands->swap(edit_commands_);
  return matched;
}

GtkWidget* Gtk2KeyBindingsHandler::CreateNewHandler() {
  Handler* handler =
      static_cast<Handler*>(g_object_new(HandlerGetType(), NULL));

  handler->owner = this;

  // We don't need to show the |handler| object on screen, so set its size to
  // zero.
  gtk_widget_set_size_request(GTK_WIDGET(handler), 0, 0);

  // Prevents it from handling any events by itself.
  gtk_widget_set_sensitive(GTK_WIDGET(handler), FALSE);
  gtk_widget_set_events(GTK_WIDGET(handler), 0);
  gtk_widget_set_can_focus(GTK_WIDGET(handler), TRUE);

  return GTK_WIDGET(handler);
}

void Gtk2KeyBindingsHandler::EditCommandMatched(
    TextEditCommandAuraLinux::CommandId id,
    const std::string& value,
    bool extend_selection) {
  edit_commands_.push_back(TextEditCommandAuraLinux(id,
                                                    value,
                                                    extend_selection));
}

void Gtk2KeyBindingsHandler::BuildGdkEventKeyFromXEvent(
    const base::NativeEvent& xevent,
    GdkEventKey* gdk_event) {
  GdkKeymap *keymap = gdk_keymap_get_for_display(gdk_display_get_default());
  GdkModifierType consumed, state;

  gdk_event->type = xevent->xany.type == KeyPress ?
                    GDK_KEY_PRESS : GDK_KEY_RELEASE;
  gdk_event->time = xevent->xkey.time;
  gdk_event->state = static_cast<GdkModifierType>(xevent->xkey.state);
  gdk_event->hardware_keycode = xevent->xkey.keycode;

  if (has_xkb_) {
    gdk_event->group = XkbGroupForCoreState(xevent->xkey.state);
  } else {
    // The overwhelming majority of people will be using X servers that support
    // XKB. GDK has a fallback here that does some complicated stuff to detect
    // whether a modifier key affects the keybinding, but that should be
    // extremely rare.
    NOTIMPLEMENTED();
    gdk_event->group = 0;
  }

  gdk_event->keyval = GDK_VoidSymbol;
  gdk_keymap_translate_keyboard_state(
      keymap,
      gdk_event->hardware_keycode,
      static_cast<GdkModifierType>(gdk_event->state),
      gdk_event->group,
      &gdk_event->keyval,
      NULL, NULL, &consumed);

  state = static_cast<GdkModifierType>(gdk_event->state & ~consumed);
  gdk_keymap_add_virtual_modifiers(keymap, &state);
  gdk_event->state |= state;
}

void Gtk2KeyBindingsHandler::HandlerInit(Handler *self) {
  self->owner = NULL;
}

void Gtk2KeyBindingsHandler::HandlerClassInit(HandlerClass *klass) {
  GtkTextViewClass* text_view_class = GTK_TEXT_VIEW_CLASS(klass);
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

  // Overrides all virtual methods related to editor key bindings.
  text_view_class->backspace = BackSpace;
  text_view_class->copy_clipboard = CopyClipboard;
  text_view_class->cut_clipboard = CutClipboard;
  text_view_class->delete_from_cursor = DeleteFromCursor;
  text_view_class->insert_at_cursor = InsertAtCursor;
  text_view_class->move_cursor = MoveCursor;
  text_view_class->paste_clipboard = PasteClipboard;
  text_view_class->set_anchor = SetAnchor;
  text_view_class->toggle_overwrite = ToggleOverwrite;
  widget_class->show_help = ShowHelp;

  // "move-focus", "move-viewport", "select-all" and "toggle-cursor-visible"
  // have no corresponding virtual methods. Since glib 2.18 (gtk 2.14),
  // g_signal_override_class_handler() is introduced to override a signal
  // handler.
  g_signal_override_class_handler("move-focus",
                                  G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(MoveFocus));

  g_signal_override_class_handler("move-viewport",
                                  G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(MoveViewport));

  g_signal_override_class_handler("select-all",
                                  G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(SelectAll));

  g_signal_override_class_handler("toggle-cursor-visible",
                                  G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(ToggleCursorVisible));
}

GType Gtk2KeyBindingsHandler::HandlerGetType() {
  static volatile gsize type_id_volatile = 0;
  if (g_once_init_enter(&type_id_volatile)) {
    GType type_id = g_type_register_static_simple(
        GTK_TYPE_TEXT_VIEW,
        g_intern_static_string("Gtk2KeyBindingsHandler"),
        sizeof(HandlerClass),
        reinterpret_cast<GClassInitFunc>(HandlerClassInit),
        sizeof(Handler),
        reinterpret_cast<GInstanceInitFunc>(HandlerInit),
        static_cast<GTypeFlags>(0));
    g_once_init_leave(&type_id_volatile, type_id);
  }
  return type_id_volatile;
}

Gtk2KeyBindingsHandler* Gtk2KeyBindingsHandler::GetHandlerOwner(
    GtkTextView* text_view) {
  Handler* handler = G_TYPE_CHECK_INSTANCE_CAST(
      text_view, HandlerGetType(), Handler);
  DCHECK(handler);
  return handler->owner;
}

void Gtk2KeyBindingsHandler::BackSpace(GtkTextView* text_view) {
  GetHandlerOwner(text_view)
      ->EditCommandMatched(
          TextEditCommandAuraLinux::DELETE_BACKWARD, std::string(), false);
}

void Gtk2KeyBindingsHandler::CopyClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(
      TextEditCommandAuraLinux::COPY, std::string(), false);
}

void Gtk2KeyBindingsHandler::CutClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(
      TextEditCommandAuraLinux::CUT, std::string(), false);
}

void Gtk2KeyBindingsHandler::DeleteFromCursor(
    GtkTextView* text_view, GtkDeleteType type, gint count) {
  if (!count)
    return;

  TextEditCommandAuraLinux::CommandId commands[2] = {
    TextEditCommandAuraLinux::INVALID_COMMAND,
    TextEditCommandAuraLinux::INVALID_COMMAND,
  };
  switch (type) {
    case GTK_DELETE_CHARS:
      commands[0] = (count > 0 ?
          TextEditCommandAuraLinux::DELETE_FORWARD :
          TextEditCommandAuraLinux::DELETE_BACKWARD);
      break;
    case GTK_DELETE_WORD_ENDS:
      commands[0] = (count > 0 ?
          TextEditCommandAuraLinux::DELETE_WORD_FORWARD :
          TextEditCommandAuraLinux::DELETE_WORD_BACKWARD);
      break;
    case GTK_DELETE_WORDS:
      if (count > 0) {
        commands[0] = TextEditCommandAuraLinux::MOVE_WORD_FORWARD;
        commands[1] = TextEditCommandAuraLinux::DELETE_WORD_BACKWARD;
      } else {
        commands[0] = TextEditCommandAuraLinux::MOVE_WORD_BACKWARD;
        commands[1] = TextEditCommandAuraLinux::DELETE_WORD_FORWARD;
      }
      break;
    case GTK_DELETE_DISPLAY_LINES:
      commands[0] = TextEditCommandAuraLinux::MOVE_TO_BEGINING_OF_LINE;
      commands[1] = TextEditCommandAuraLinux::DELETE_TO_END_OF_LINE;
      break;
    case GTK_DELETE_DISPLAY_LINE_ENDS:
      commands[0] = (count > 0 ?
          TextEditCommandAuraLinux::DELETE_TO_END_OF_LINE :
          TextEditCommandAuraLinux::DELETE_TO_BEGINING_OF_LINE);
      break;
    case GTK_DELETE_PARAGRAPH_ENDS:
      commands[0] = (count > 0 ?
          TextEditCommandAuraLinux::DELETE_TO_END_OF_PARAGRAPH :
          TextEditCommandAuraLinux::DELETE_TO_BEGINING_OF_PARAGRAPH);
      break;
    case GTK_DELETE_PARAGRAPHS:
      commands[0] =
          TextEditCommandAuraLinux::MOVE_TO_BEGINING_OF_PARAGRAPH;
      commands[1] =
          TextEditCommandAuraLinux::DELETE_TO_END_OF_PARAGRAPH;
      break;
    default:
      // GTK_DELETE_WHITESPACE has no corresponding editor command.
      return;
  }

  Gtk2KeyBindingsHandler* owner = GetHandlerOwner(text_view);
  if (count < 0)
    count = -count;
  for (; count > 0; --count) {
    for (size_t i = 0; i < arraysize(commands); ++i)
      if (commands[i] != TextEditCommandAuraLinux::INVALID_COMMAND)
        owner->EditCommandMatched(commands[i], std::string(), false);
  }
}

void Gtk2KeyBindingsHandler::InsertAtCursor(GtkTextView* text_view,
                                            const gchar* str) {
  if (str && *str)
    GetHandlerOwner(text_view)->EditCommandMatched(
        TextEditCommandAuraLinux::INSERT_TEXT, str, false);
}

void Gtk2KeyBindingsHandler::MoveCursor(
    GtkTextView* text_view, GtkMovementStep step, gint count,
    gboolean extend_selection) {
  if (!count)
    return;

  TextEditCommandAuraLinux::CommandId command;
  switch (step) {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
      command = (count > 0 ?
                 TextEditCommandAuraLinux::MOVE_FORWARD :
                 TextEditCommandAuraLinux::MOVE_BACKWARD);
      break;
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      command = (count > 0 ?
                 TextEditCommandAuraLinux::MOVE_RIGHT :
                 TextEditCommandAuraLinux::MOVE_LEFT);
      break;
    case GTK_MOVEMENT_WORDS:
      command = (count > 0 ?
                 TextEditCommandAuraLinux::MOVE_WORD_RIGHT :
                 TextEditCommandAuraLinux::MOVE_WORD_LEFT);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      command = (count > 0 ?
                 TextEditCommandAuraLinux::MOVE_DOWN :
                 TextEditCommandAuraLinux::MOVE_UP);
      break;
    case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
      command = (count > 0 ?
                 TextEditCommandAuraLinux::MOVE_TO_END_OF_LINE :
                 TextEditCommandAuraLinux::MOVE_TO_BEGINING_OF_LINE);
      break;
    case GTK_MOVEMENT_PARAGRAPH_ENDS:
      command = (count > 0 ?
                 TextEditCommandAuraLinux::MOVE_TO_END_OF_PARAGRAPH :
                 TextEditCommandAuraLinux::MOVE_TO_BEGINING_OF_PARAGRAPH);
      break;
    case GTK_MOVEMENT_PAGES:
      command = (count > 0 ? TextEditCommandAuraLinux::MOVE_PAGE_DOWN :
                 TextEditCommandAuraLinux::MOVE_PAGE_UP);
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      command = (count > 0 ? TextEditCommandAuraLinux::MOVE_TO_END_OF_DOCUMENT :
                 TextEditCommandAuraLinux::MOVE_TO_BEGINING_OF_DOCUMENT);
      break;
    default:
      // GTK_MOVEMENT_PARAGRAPHS and GTK_MOVEMENT_HORIZONTAL_PAGES have
      // no corresponding editor commands.
      return;
  }

  Gtk2KeyBindingsHandler* owner = GetHandlerOwner(text_view);
  if (count < 0)
    count = -count;
  for (; count > 0; --count)
    owner->EditCommandMatched(command, std::string(), extend_selection);
}

void Gtk2KeyBindingsHandler::MoveViewport(
    GtkTextView* text_view, GtkScrollStep step, gint count) {
  // Not supported by webkit.
}

void Gtk2KeyBindingsHandler::PasteClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(
      TextEditCommandAuraLinux::PASTE, std::string(), false);
}

void Gtk2KeyBindingsHandler::SelectAll(GtkTextView* text_view,
                                       gboolean select) {
  if (select) {
    GetHandlerOwner(text_view)->EditCommandMatched(
        TextEditCommandAuraLinux::SELECT_ALL, std::string(), false);
  } else {
    GetHandlerOwner(text_view)->EditCommandMatched(
        TextEditCommandAuraLinux::UNSELECT, std::string(), false);
  }
}

void Gtk2KeyBindingsHandler::SetAnchor(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(
      TextEditCommandAuraLinux::SET_MARK, std::string(), false);
}

void Gtk2KeyBindingsHandler::ToggleCursorVisible(GtkTextView* text_view) {
  // Not supported by webkit.
}

void Gtk2KeyBindingsHandler::ToggleOverwrite(GtkTextView* text_view) {
  // Not supported by webkit.
}

gboolean Gtk2KeyBindingsHandler::ShowHelp(GtkWidget* widget,
                                          GtkWidgetHelpType arg1) {
  // Just for disabling the default handler.
  return FALSE;
}

void Gtk2KeyBindingsHandler::MoveFocus(GtkWidget* widget,
                                       GtkDirectionType arg1) {
  // Just for disabling the default handler.
}

}  // namespace libgtk2ui

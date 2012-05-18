// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gtk_key_bindings_handler.h"

#include <gdk/gdkkeysyms.h>

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "content/public/browser/native_web_keyboard_event.h"

using content::NativeWebKeyboardEvent;

GtkKeyBindingsHandler::GtkKeyBindingsHandler(GtkWidget* parent_widget)
    : handler_(CreateNewHandler()) {
  DCHECK(GTK_IS_FIXED(parent_widget));
  // We need add the |handler_| object into gtk widget hierarchy, so that
  // gtk_bindings_activate_event() can find correct display and keymaps from
  // the |handler_| object.
  gtk_fixed_put(GTK_FIXED(parent_widget), handler_.get(), -1, -1);
}

GtkKeyBindingsHandler::~GtkKeyBindingsHandler() {
  handler_.Destroy();
}

bool GtkKeyBindingsHandler::Match(const NativeWebKeyboardEvent& wke,
                                  EditCommands* edit_commands) {
  if (wke.type == WebKit::WebInputEvent::Char || !wke.os_event)
    return false;

  edit_commands_.clear();
  // If this key event matches a predefined key binding, corresponding signal
  // will be emitted.
  gtk_bindings_activate_event(GTK_OBJECT(handler_.get()), &wke.os_event->key);

  bool matched = !edit_commands_.empty();
  if (edit_commands)
    edit_commands->swap(edit_commands_);
  return matched;
}

GtkWidget* GtkKeyBindingsHandler::CreateNewHandler() {
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

void GtkKeyBindingsHandler::EditCommandMatched(
    const std::string& name, const std::string& value) {
  edit_commands_.push_back(EditCommand(name, value));
}

void GtkKeyBindingsHandler::HandlerInit(Handler *self) {
  self->owner = NULL;
}

void GtkKeyBindingsHandler::HandlerClassInit(HandlerClass *klass) {
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

GType GtkKeyBindingsHandler::HandlerGetType() {
  static volatile gsize type_id_volatile = 0;
  if (g_once_init_enter(&type_id_volatile)) {
    GType type_id = g_type_register_static_simple(
        GTK_TYPE_TEXT_VIEW,
        g_intern_static_string("GtkKeyBindingsHandler"),
        sizeof(HandlerClass),
        reinterpret_cast<GClassInitFunc>(HandlerClassInit),
        sizeof(Handler),
        reinterpret_cast<GInstanceInitFunc>(HandlerInit),
        static_cast<GTypeFlags>(0));
    g_once_init_leave(&type_id_volatile, type_id);
  }
  return type_id_volatile;
}

GtkKeyBindingsHandler* GtkKeyBindingsHandler::GetHandlerOwner(
    GtkTextView* text_view) {
  Handler* handler = G_TYPE_CHECK_INSTANCE_CAST(
      text_view, HandlerGetType(), Handler);
  DCHECK(handler);
  return handler->owner;
}

void GtkKeyBindingsHandler::BackSpace(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched("DeleteBackward", "");
}

void GtkKeyBindingsHandler::CopyClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched("Copy", "");
}

void GtkKeyBindingsHandler::CutClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched("Cut", "");
}

void GtkKeyBindingsHandler::DeleteFromCursor(
    GtkTextView* text_view, GtkDeleteType type, gint count) {
  if (!count)
    return;

  const char *commands[3] = { NULL, NULL, NULL };
  switch (type) {
    case GTK_DELETE_CHARS:
      commands[0] = (count > 0 ? "DeleteForward" : "DeleteBackward");
      break;
    case GTK_DELETE_WORD_ENDS:
      commands[0] = (count > 0 ? "DeleteWordForward" : "DeleteWordBackward");
      break;
    case GTK_DELETE_WORDS:
      if (count > 0) {
        commands[0] = "MoveWordForward";
        commands[1] = "DeleteWordBackward";
      } else {
        commands[0] = "MoveWordBackward";
        commands[1] = "DeleteWordForward";
      }
      break;
    case GTK_DELETE_DISPLAY_LINES:
      commands[0] = "MoveToBeginningOfLine";
      commands[1] = "DeleteToEndOfLine";
      break;
    case GTK_DELETE_DISPLAY_LINE_ENDS:
      commands[0] = (count > 0 ? "DeleteToEndOfLine" :
                     "DeleteToBeginningOfLine");
      break;
    case GTK_DELETE_PARAGRAPH_ENDS:
      commands[0] = (count > 0 ? "DeleteToEndOfParagraph" :
                     "DeleteToBeginningOfParagraph");
      break;
    case GTK_DELETE_PARAGRAPHS:
      commands[0] = "MoveToBeginningOfParagraph";
      commands[1] = "DeleteToEndOfParagraph";
      break;
    default:
      // GTK_DELETE_WHITESPACE has no corresponding editor command.
      return;
  }

  GtkKeyBindingsHandler* owner = GetHandlerOwner(text_view);
  if (count < 0)
    count = -count;
  for (; count > 0; --count) {
    for (const char* const* p = commands; *p; ++p)
      owner->EditCommandMatched(*p, "");
  }
}

void GtkKeyBindingsHandler::InsertAtCursor(GtkTextView* text_view,
                                           const gchar* str) {
  if (str && *str)
    GetHandlerOwner(text_view)->EditCommandMatched("InsertText", str);
}

void GtkKeyBindingsHandler::MoveCursor(
    GtkTextView* text_view, GtkMovementStep step, gint count,
    gboolean extend_selection) {
  if (!count)
    return;

  std::string command;
  switch (step) {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
      command = (count > 0 ? "MoveForward" : "MoveBackward");
      break;
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      command = (count > 0 ? "MoveRight" : "MoveLeft");
      break;
    case GTK_MOVEMENT_WORDS:
      command = (count > 0 ? "MoveWordRight" : "MoveWordLeft");
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      command = (count > 0 ? "MoveDown" : "MoveUp");
      break;
    case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
      command = (count > 0 ? "MoveToEndOfLine" : "MoveToBeginningOfLine");
      break;
    case GTK_MOVEMENT_PARAGRAPH_ENDS:
      command = (count > 0 ? "MoveToEndOfParagraph" :
                 "MoveToBeginningOfParagraph");
      break;
    case GTK_MOVEMENT_PAGES:
      command = (count > 0 ? "MovePageDown" : "MovePageUp");
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      command = (count > 0 ? "MoveToEndOfDocument" :
                 "MoveToBeginningOfDocument");
      break;
    default:
      // GTK_MOVEMENT_PARAGRAPHS and GTK_MOVEMENT_HORIZONTAL_PAGES have
      // no corresponding editor commands.
      return;
  }

  GtkKeyBindingsHandler* owner = GetHandlerOwner(text_view);
  if (extend_selection)
    command.append("AndModifySelection");
  if (count < 0)
    count = -count;
  for (; count > 0; --count)
    owner->EditCommandMatched(command, "");
}

void GtkKeyBindingsHandler::MoveViewport(
    GtkTextView* text_view, GtkScrollStep step, gint count) {
  // Not supported by webkit.
}

void GtkKeyBindingsHandler::PasteClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched("Paste", "");
}

void GtkKeyBindingsHandler::SelectAll(GtkTextView* text_view, gboolean select) {
  if (select)
    GetHandlerOwner(text_view)->EditCommandMatched("SelectAll", "");
  else
    GetHandlerOwner(text_view)->EditCommandMatched("Unselect", "");
}

void GtkKeyBindingsHandler::SetAnchor(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched("SetMark", "");
}

void GtkKeyBindingsHandler::ToggleCursorVisible(GtkTextView* text_view) {
  // Not supported by webkit.
}

void GtkKeyBindingsHandler::ToggleOverwrite(GtkTextView* text_view) {
  // Not supported by webkit.
}

gboolean GtkKeyBindingsHandler::ShowHelp(GtkWidget* widget,
                                         GtkWidgetHelpType arg1) {
  // Just for disabling the default handler.
  return FALSE;
}

void GtkKeyBindingsHandler::MoveFocus(GtkWidget* widget,
                                      GtkDirectionType arg1) {
  // Just for disabling the default handler.
}

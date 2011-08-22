// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/renderer_host/gtk_key_bindings_handler.h"

#include <gdk/gdkkeysyms.h>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/common/edit_command.h"
#include "content/common/native_web_keyboard_event.h"
#include "testing/gtest/include/gtest/gtest.h"

class GtkKeyBindingsHandlerTest : public testing::Test {
 protected:
  struct EditCommand {
    const char* name;
    const char* value;
  };

  GtkKeyBindingsHandlerTest()
      : window_(gtk_window_new(GTK_WINDOW_TOPLEVEL)),
        handler_(NULL) {
    FilePath gtkrc;
    PathService::Get(chrome::DIR_TEST_DATA, &gtkrc);
    gtkrc = gtkrc.AppendASCII("gtk_key_bindings_test_gtkrc");

    gtk_rc_parse(gtkrc.value().c_str());

    GtkWidget* fixed = gtk_fixed_new();
    handler_ = new GtkKeyBindingsHandler(fixed);
    gtk_container_add(GTK_CONTAINER(window_), fixed);
    gtk_widget_show(fixed);
    gtk_widget_show(window_);
  }
  ~GtkKeyBindingsHandlerTest() {
    gtk_widget_destroy(window_);
    delete handler_;
  }

  NativeWebKeyboardEvent NewNativeWebKeyboardEvent(guint keyval, guint state) {
    GdkKeymap* keymap =
        gdk_keymap_get_for_display(gtk_widget_get_display(window_));

    GdkKeymapKey *keys = NULL;
    gint n_keys = 0;
    if (gdk_keymap_get_entries_for_keyval(keymap, keyval, &keys, &n_keys)) {
      GdkEventKey event;
      event.type = GDK_KEY_PRESS;
      event.window = NULL;
      event.send_event = 0;
      event.time = 0;
      event.state = state;
      event.keyval = keyval;
      event.length = 0;
      event.string = NULL;
      event.hardware_keycode = keys[0].keycode;
      event.group = keys[0].group;
      event.is_modifier = 0;
      g_free(keys);
      return NativeWebKeyboardEvent(&event);
    }
    LOG(ERROR) << "Failed to create key event for keyval:" << keyval;
    return NativeWebKeyboardEvent();
  }

  void TestKeyBinding(const NativeWebKeyboardEvent& event,
                      const EditCommand expected_result[],
                      size_t size) {
    EditCommands result;
    ASSERT_TRUE(handler_->Match(event, &result));
    ASSERT_EQ(size, result.size());
    for (size_t i = 0; i < size; ++i) {
      ASSERT_STREQ(expected_result[i].name, result[i].name.c_str());
      ASSERT_STREQ(expected_result[i].value, result[i].value.c_str());
    }
  }

 protected:
  GtkWidget* window_;
  GtkKeyBindingsHandler* handler_;
};

TEST_F(GtkKeyBindingsHandlerTest, MoveCursor) {
  static const EditCommand kEditCommands[] = {
    // "move-cursor" (logical-positions, -2, 0)
    { "MoveBackward", "" },
    { "MoveBackward", "" },
    // "move-cursor" (logical-positions, 2, 0)
    { "MoveForward", "" },
    { "MoveForward", "" },
    // "move-cursor" (visual-positions, -1, 1)
    { "MoveLeftAndModifySelection", "" },
    // "move-cursor" (visual-positions, 1, 1)
    { "MoveRightAndModifySelection", "" },
    // "move-cursor" (words, -1, 0)
    { "MoveWordBackward", "" },
    // "move-cursor" (words, 1, 0)
    { "MoveWordForward", "" },
    // "move-cursor" (display-lines, -1, 0)
    { "MoveUp", "" },
    // "move-cursor" (display-lines, 1, 0)
    { "MoveDown", "" },
    // "move-cursor" (display-line-ends, -1, 0)
    { "MoveToBeginningOfLine", "" },
    // "move-cursor" (display-line-ends, 1, 0)
    { "MoveToEndOfLine", "" },
    // "move-cursor" (paragraph-ends, -1, 0)
    { "MoveToBeginningOfParagraph", "" },
    // "move-cursor" (paragraph-ends, 1, 0)
    { "MoveToEndOfParagraph", "" },
    // "move-cursor" (pages, -1, 0)
    { "MovePageUp", "" },
    // "move-cursor" (pages, 1, 0)
    { "MovePageDown", "" },
    // "move-cursor" (buffer-ends, -1, 0)
    { "MoveToBeginningOfDocument", "" },
    // "move-cursor" (buffer-ends, 1, 0)
    { "MoveToEndOfDocument", "" }
  };

  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_1, GDK_CONTROL_MASK),
                 kEditCommands, arraysize(kEditCommands));
}

TEST_F(GtkKeyBindingsHandlerTest, DeleteFromCursor) {
  static const EditCommand kEditCommands[] = {
    // "delete-from-cursor" (chars, -2)
    { "DeleteBackward", "" },
    { "DeleteBackward", "" },
    // "delete-from-cursor" (chars, 2)
    { "DeleteForward", "" },
    { "DeleteForward", "" },
    // "delete-from-cursor" (word-ends, -1)
    { "DeleteWordBackward", "" },
    // "delete-from-cursor" (word-ends, 1)
    { "DeleteWordForward", "" },
    // "delete-from-cursor" (words, -1)
    { "MoveWordBackward", "" },
    { "DeleteWordForward", "" },
    // "delete-from-cursor" (words, 1)
    { "MoveWordForward", "" },
    { "DeleteWordBackward", "" },
    // "delete-from-cursor" (display-lines, -1)
    { "MoveToBeginningOfLine", "" },
    { "DeleteToEndOfLine", "" },
    // "delete-from-cursor" (display-lines, 1)
    { "MoveToBeginningOfLine", "" },
    { "DeleteToEndOfLine", "" },
    // "delete-from-cursor" (display-line-ends, -1)
    { "DeleteToBeginningOfLine", "" },
    // "delete-from-cursor" (display-line-ends, 1)
    { "DeleteToEndOfLine", "" },
    // "delete-from-cursor" (paragraph-ends, -1)
    { "DeleteToBeginningOfParagraph", "" },
    // "delete-from-cursor" (paragraph-ends, 1)
    { "DeleteToEndOfParagraph", "" },
    // "delete-from-cursor" (paragraphs, -1)
    { "MoveToBeginningOfParagraph", "" },
    { "DeleteToEndOfParagraph", "" },
    // "delete-from-cursor" (paragraphs, 1)
    { "MoveToBeginningOfParagraph", "" },
    { "DeleteToEndOfParagraph", "" },
  };

  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_2, GDK_CONTROL_MASK),
                 kEditCommands, arraysize(kEditCommands));
}

TEST_F(GtkKeyBindingsHandlerTest, OtherActions) {
  static const EditCommand kBackspace[] = {
    { "DeleteBackward", "" }
  };
  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_3, GDK_CONTROL_MASK),
                 kBackspace, arraysize(kBackspace));

  static const EditCommand kCopyClipboard[] = {
    { "Copy", "" }
  };
  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_4, GDK_CONTROL_MASK),
                 kCopyClipboard, arraysize(kCopyClipboard));

  static const EditCommand kCutClipboard[] = {
    { "Cut", "" }
  };
  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_5, GDK_CONTROL_MASK),
                 kCutClipboard, arraysize(kCutClipboard));

  static const EditCommand kInsertAtCursor[] = {
    { "InsertText", "hello" }
  };
  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_6, GDK_CONTROL_MASK),
                 kInsertAtCursor, arraysize(kInsertAtCursor));

  static const EditCommand kPasteClipboard[] = {
    { "Paste", "" }
  };
  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_7, GDK_CONTROL_MASK),
                 kPasteClipboard, arraysize(kPasteClipboard));

  static const EditCommand kSelectAll[] = {
    { "Unselect", "" },
    { "SelectAll", "" }
  };
  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_8, GDK_CONTROL_MASK),
                 kSelectAll, arraysize(kSelectAll));

  static const EditCommand kSetAnchor[] = {
    { "SetMark", "" }
  };
  TestKeyBinding(NewNativeWebKeyboardEvent(GDK_9, GDK_CONTROL_MASK),
                 kSetAnchor, arraysize(kSetAnchor));
}

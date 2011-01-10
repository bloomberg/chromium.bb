// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIPBOARD_DISPATCHER_H_
#define CHROME_BROWSER_CLIPBOARD_DISPATCHER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/base/clipboard/clipboard.h"

// This class backs IPC requests from the renderer for clipboard data. In this
// context, clipboard does not only refer to the usual concept of a clipboard
// for copy/paste, which is why it's not in app/clipboard/clipboard.h. It can
// refer to one of three different types of clipboards:
// - The copy/paste clipboard, which contains data that has been copied/cut.
// - The dragging clipboard, which contains data that is currently being
//   dragged.
// - On X, the selection clipboard, which contains data for the current
//   selection.
class ClipboardDispatcher {
 public:
  static bool ReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                 std::vector<string16>* types,
                                 bool* contains_filenames);
  static bool ReadData(ui::Clipboard::Buffer buffer, const string16& type,
                       string16* data, string16* metadata);
  static bool ReadFilenames(ui::Clipboard::Buffer buffer,
                            std::vector<string16>* filenames);

 private:
  // This class is not meant to be instantiated. All public members are static.
  ClipboardDispatcher();

  DISALLOW_COPY_AND_ASSIGN(ClipboardDispatcher);
};

#endif  // CHROME_BROWSER_CLIPBOARD_DISPATCHER_H_

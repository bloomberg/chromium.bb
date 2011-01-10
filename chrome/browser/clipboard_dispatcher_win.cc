// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/clipboard_dispatcher.h"
#include "base/logging.h"

bool ClipboardDispatcher::ReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                             std::vector<string16>* types,
                                             bool* contains_filenames) {
  DCHECK(types);
  DCHECK(contains_filenames);
  types->clear();
  *contains_filenames = false;
  return false;
}

bool ClipboardDispatcher::ReadData(ui::Clipboard::Buffer buffer,
                                   const string16& type,
                                   string16* data,
                                   string16* metadata) {
  DCHECK(data);
  DCHECK(metadata);
  return false;
}

bool ClipboardDispatcher::ReadFilenames(ui::Clipboard::Buffer buffer,
                                        std::vector<string16>* filenames) {
  DCHECK(filenames);
  filenames->clear();
  return false;
}

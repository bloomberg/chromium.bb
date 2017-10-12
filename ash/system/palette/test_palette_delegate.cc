// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/test_palette_delegate.h"

#include "base/callback_helpers.h"

namespace ash {

TestPaletteDelegate::TestPaletteDelegate() {}

TestPaletteDelegate::~TestPaletteDelegate() {}

std::unique_ptr<PaletteDelegate::EnableListenerSubscription>
TestPaletteDelegate::AddPaletteEnableListener(
    const EnableListener& on_state_changed) {
  return nullptr;
}

void TestPaletteDelegate::CreateNote() {
  ++create_note_count_;
}

bool TestPaletteDelegate::HasNoteApp() {
  ++has_note_app_count_;
  return has_note_app_;
}

bool TestPaletteDelegate::ShouldAutoOpenPalette() {
  return should_auto_open_palette_;
}

bool TestPaletteDelegate::ShouldShowPalette() {
  return should_show_palette_;
}

}  // namespace ash

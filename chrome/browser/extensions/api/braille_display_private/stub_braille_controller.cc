// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"

namespace extensions {
namespace api {
namespace braille_display_private {

StubBrailleController::StubBrailleController() {
}

scoped_ptr<DisplayState> StubBrailleController::GetDisplayState() {
  return scoped_ptr<DisplayState>(new DisplayState);
}

void StubBrailleController::WriteDots(const std::string& cells) {
}

void StubBrailleController::AddObserver(BrailleObserver* observer) {
}

void StubBrailleController::RemoveObserver(BrailleObserver* observer) {
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

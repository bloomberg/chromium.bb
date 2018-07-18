// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_support.h"

namespace vr {
namespace ui_support {

UScriptCode UScriptGetScript(UChar32 codepoint, UErrorCode* err) {
  return uscript_getScript(codepoint, err);
}

}  // namespace ui_support
}  // namespace vr

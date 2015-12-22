// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_DESKTOP_UI_BROWSER_COMMANDS_H_
#define MANDOLINE_UI_DESKTOP_UI_BROWSER_COMMANDS_H_

#include <stdint.h>

namespace mandoline {

enum class BrowserCommand : uint32_t {
  CLOSE,
  FOCUS_OMNIBOX,
  NEW_WINDOW,
  SHOW_FIND,
  GO_BACK,
  GO_FORWARD,
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_DESKTOP_UI_BROWSER_COMMANDS_H_

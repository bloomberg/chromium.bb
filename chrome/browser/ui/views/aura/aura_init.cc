// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/aura_init.h"

#include "ui/aura_shell/shell_factory.h"

namespace browser {

void InitAuraDesktop() {
  aura_shell::InitDesktopWindow();
}

}  // namespace browser

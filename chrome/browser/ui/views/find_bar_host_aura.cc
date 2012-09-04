// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_host.h"

#include "base/logging.h"
#include "ui/base/events/event.h"

void FindBarHost::AudibleAlert() {
#if defined(OS_WIN)
  MessageBeep(MB_OK);
#else
  // TODO(mukai):
  NOTIMPLEMENTED();
#endif
}

bool FindBarHost::ShouldForwardKeyEventToWebpageNative(
    const ui::KeyEvent& key_event) {
  return true;
}

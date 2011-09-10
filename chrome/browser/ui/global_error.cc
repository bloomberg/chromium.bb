// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error.h"

#include "base/logging.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"

GlobalError::GlobalError() {
}

GlobalError::~GlobalError() {
}

int GlobalError::GetBadgeResourceID() {
  return IDR_UPDATE_BADGE4;
}

int GlobalError::MenuItemIconResourceID() {
  return IDR_UPDATE_MENU4;
}

void GlobalError::ShowBubbleView(Browser* browser) {
  ShowBubbleView(browser, this);
}

int GlobalError::GetBubbleViewIconResourceID() {
  return IDR_INPUT_ALERT;
}

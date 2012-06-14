// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"

CoreTabHelperDelegate::~CoreTabHelperDelegate() {
}

void CoreTabHelperDelegate::SwapTabContents(TabContents* old_tc,
                                            TabContents* new_tc) {
}

bool CoreTabHelperDelegate::CanReloadContents(TabContents* source) const {
  return true;
}

bool CoreTabHelperDelegate::CanSaveContents(TabContents* source) const {
  return true;
}

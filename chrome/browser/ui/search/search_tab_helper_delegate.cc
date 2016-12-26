// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper_delegate.h"

void SearchTabHelperDelegate::OnWebContentsInstantSupportDisabled(
    const content::WebContents* web_contents) {
}

OmniboxView* SearchTabHelperDelegate::GetOmniboxView() {
  return nullptr;
}

SearchTabHelperDelegate::~SearchTabHelperDelegate() {
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_contents_modal_dialog_manager_delegate.h"

#include <string.h>

bool WebContentsModalDialogManagerDelegate::ShouldFocusWebContentsModalDialog(
) {
  return true;
}

void WebContentsModalDialogManagerDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents, bool blocked) {
}

WebContentsModalDialogManagerDelegate::~WebContentsModalDialogManagerDelegate(
) {}

bool WebContentsModalDialogManagerDelegate::GetDialogTopCenter(
    gfx::Point* point) {
  return false;
}

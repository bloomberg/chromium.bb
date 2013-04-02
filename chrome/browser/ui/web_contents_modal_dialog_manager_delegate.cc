// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_contents_modal_dialog_manager_delegate.h"

#include <string.h>

void WebContentsModalDialogManagerDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents, bool blocked) {
}

WebContentsModalDialogHost*
    WebContentsModalDialogManagerDelegate::GetWebContentsModalDialogHost() {
  return NULL;
}

WebContentsModalDialogManagerDelegate::~WebContentsModalDialogManagerDelegate(
) {}

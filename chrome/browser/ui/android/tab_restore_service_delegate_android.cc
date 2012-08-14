// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_delegate.h"

#include "content/public/browser/navigation_controller.h"

// static
TabRestoreServiceDelegate*
    TabRestoreServiceDelegate::FindDelegateForWebContents(
        const content::WebContents* contents) {
  // We don't restore tabs using TabRestoreService yet.
  return NULL;
}

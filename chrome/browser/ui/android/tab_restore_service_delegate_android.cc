// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tabs persistence is implemented on the Java side.

#include "chrome/browser/sessions/tab_restore_service_delegate.h"

#include "content/public/browser/navigation_controller.h"

// static
TabRestoreServiceDelegate* TabRestoreServiceDelegate::Create(
    Profile* profile,
    chrome::HostDesktopType host_desktop_type,
    const std::string& app_name) {
  return NULL;
}

// static
TabRestoreServiceDelegate*
    TabRestoreServiceDelegate::FindDelegateForWebContents(
        const content::WebContents* contents) {
  return NULL;
}

// static
TabRestoreServiceDelegate* TabRestoreServiceDelegate::FindDelegateWithID(
    SessionID::id_type desired_id,
    chrome::HostDesktopType host_desktop_type) {
  return NULL;
}

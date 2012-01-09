// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/shell_window.h"

#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

ShellWindow::ShellWindow(ExtensionHost* host)
    : host_(host) {
  // Close the window in response to window.close() and the like.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(host->profile()));
}

ShellWindow::~ShellWindow() {
}

ShellWindow* ShellWindow::Create(Profile* profile,
                                 const Extension* extension,
                                 const GURL& url) {
  ExtensionProcessManager* manager = profile->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;

  ExtensionHost* host = manager->CreateShellHost(extension, url);
  // CHECK host so that non-GTK platform compilers don't complain about unused
  // variables.
  // TODO(mihaip): remove when ShellWindow has been implemented everywhere.
  CHECK(host);

#if defined(TOOLKIT_GTK)
  // This object will delete itself when the window is closed.
  // TODO(mihaip): remove the #if block when ShellWindow has been implemented
  // everywhere.
  return ShellWindow::CreateShellWindow(host);
#endif

  return NULL;
}

void ShellWindow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      if (content::Details<ExtensionHost>(host_.get()) == details)
        Close();
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/platform_app_service.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace extensions {

PlatformAppService::PlatformAppService(Profile* profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
                 content::Source<content::BrowserContext>(profile));
}

PlatformAppService::~PlatformAppService() {}

void PlatformAppService::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_CREATED: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      if (host->extension()->is_platform_app())
        host->SetKeepsBrowserProcessAlive();
      break;
    }
    default:
      NOTREACHED();
    }
}

}  // namespace extensions

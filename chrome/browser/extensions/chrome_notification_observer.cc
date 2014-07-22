// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_notification_observer.h"

#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension_messages.h"

namespace extensions {

ChromeNotificationObserver::ChromeNotificationObserver() {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ChromeNotificationObserver::~ChromeNotificationObserver() {}

void ChromeNotificationObserver::OnRendererProcessCreated(
    content::RenderProcessHost* process) {
  // Extensions need to know the channel for API restrictions. Send the channel
  // to all renderers, as the non-extension renderers may have content scripts.
  process->Send(new ExtensionMsg_SetChannel(GetCurrentChannel()));
}

void ChromeNotificationObserver::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      OnRendererProcessCreated(process);
      break;
    }

    default:
      NOTREACHED();
  }
}

}  // namespace extensions

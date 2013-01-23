// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaGalleryExtensionNotificationObserver implementation.

#include "chrome/browser/extensions/api/media_galleries_private/media_gallery_extension_notification_observer.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/extensions/api/media_galleries_private/gallery_watch_manager.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

using content::BrowserThread;

namespace {

// Handles the extension destroyed event on the file thread.
void HandleExtensionDestroyedOnFileThread(void* profile_id,
                                          const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!GalleryWatchManager::HasForProfile(profile_id))
    return;
  GalleryWatchManager::GetForProfile(profile_id)->OnExtensionDestroyed(
      extension_id);
}

}  // namespace

MediaGalleryExtensionNotificationObserver::
MediaGalleryExtensionNotificationObserver(Profile* profile)
    : profile_(profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
}

MediaGalleryExtensionNotificationObserver::
~MediaGalleryExtensionNotificationObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void MediaGalleryExtensionNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Extension* extension = NULL;
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    extension = const_cast<Extension*>(
        content::Details<extensions::UnloadedExtensionInfo>(
            details)->extension);
    DCHECK(extension);
  } else {
    DCHECK(type == chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED);
    ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
    extension = const_cast<Extension*>(host->extension());
    if (!extension)
      return;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&HandleExtensionDestroyedOnFileThread,
                 profile_,
                 extension->id()));
}

}  // namespace extensions

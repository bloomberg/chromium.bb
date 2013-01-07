// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class observes media gallery extension notification events. This
// class object is created, destructed and operated on the UI thread.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERY_EXTENSION_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERY_EXTENSION_NOTIFICATION_OBSERVER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

class MediaGalleryExtensionNotificationObserver
    : public content::NotificationObserver {
 public:
  explicit MediaGalleryExtensionNotificationObserver(Profile* profile);
  virtual ~MediaGalleryExtensionNotificationObserver();

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  // Current profile.
  Profile* profile_;

  // Used to listen for NOTIFICATION_EXTENSION_UNLOADED and
  // NOTIFICATION_EXTENSION_HOST_DESTROYED events.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleryExtensionNotificationObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERY_EXTENSION_NOTIFICATION_OBSERVER_H_

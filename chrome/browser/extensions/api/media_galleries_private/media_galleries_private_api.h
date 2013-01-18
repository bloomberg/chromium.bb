// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_API_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"

class FilePath;
class Profile;

namespace extensions {

class MediaGalleryExtensionNotificationObserver;
class MediaGalleriesPrivateEventRouter;

// The profile-keyed service that manages the media galleries private extension
// API.
class MediaGalleriesPrivateAPI : public ProfileKeyedAPI,
                                 public EventRouter::Observer {
 public:
  explicit MediaGalleriesPrivateAPI(Profile* profile);
  virtual ~MediaGalleriesPrivateAPI();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<MediaGalleriesPrivateAPI>* GetFactoryInstance();

  // Convenience method to get the MediaGalleriesPrivateAPI for a profile.
  static MediaGalleriesPrivateAPI* Get(Profile* profile);

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

  MediaGalleriesPrivateEventRouter* GetEventRouter();

 private:
  friend class ProfileKeyedAPIFactory<MediaGalleriesPrivateAPI>;

  void MaybeInitializeEventRouter();

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "MediaGalleriesPrivateAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Current profile.
  Profile* profile_;

  scoped_ptr<MediaGalleryExtensionNotificationObserver>
      extension_notification_observer_;

  // Created lazily on first access.
  scoped_ptr<MediaGalleriesPrivateEventRouter>
      media_galleries_private_event_router_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPrivateAPI);
};

// Implements the chrome.mediaGalleriesPrivate.addGalleryWatch method.
class MediaGalleriesPrivateAddGalleryWatchFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleriesPrivate.addGalleryWatch",
                             MEDIAGALLERIESPRIVATE_ADDGALLERYWATCH);

 protected:
  virtual ~MediaGalleriesPrivateAddGalleryWatchFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Gallery watch request handler.
  void HandleResponse(chrome::MediaGalleryPrefId gallery_id,
                      bool success);
};

// Implements the chrome.mediaGalleriesPrivate.removeGalleryWatch method.
class MediaGalleriesPrivateRemoveGalleryWatchFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleriesPrivate.removeGalleryWatch",
                             MEDIAGALLERIESPRIVATE_REMOVEGALLERYWATCH);

 protected:
  virtual ~MediaGalleriesPrivateRemoveGalleryWatchFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_API_H_

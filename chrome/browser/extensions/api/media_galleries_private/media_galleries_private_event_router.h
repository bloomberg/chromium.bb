// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Manages events for the private Chrome extensions media galleries API. This
// is temporary and will be moved to a permanent, public place in the near
// future. This class object lives on the UI thread.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_EVENT_ROUTER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"

class FilePath;
class Profile;

namespace base {
class ListValue;
}

namespace extensions {

class MediaGalleriesPrivateEventRouter
    : public base::SystemMonitor::DevicesChangedObserver,
      public base::SupportsWeakPtr<MediaGalleriesPrivateEventRouter> {
 public:
  explicit MediaGalleriesPrivateEventRouter(Profile* profile);
  virtual ~MediaGalleriesPrivateEventRouter();

  // Gallery changed event handler.
  void OnGalleryChanged(chrome::MediaGalleryPrefId gallery_id,
                        const std::set<std::string>& extension_ids);

 private:
  // base::SystemMonitor::DevicesChangedObserver implementation.
  virtual void OnRemovableStorageAttached(
      const std::string& id,
      const string16& name,
      const FilePath::StringType& location) OVERRIDE;
  virtual void OnRemovableStorageDetached(const std::string& id) OVERRIDE;

  void DispatchEvent(const std::string& event_name,
                     scoped_ptr<base::ListValue> event_args);

  // Current profile.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_EVENT_ROUTER_H_

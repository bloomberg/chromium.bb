// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_MEDIA_GALLERY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_MEDIA_GALLERY_HANDLER_H_

#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/webui/options2/options_ui.h"
#include "content/public/browser/notification_observer.h"

namespace options2 {

// Handles messages related to adding or removing media galleries.
class MediaGalleryHandler : public OptionsPageUIHandler,
                            public SelectFileDialog::Listener {
 public:
  MediaGalleryHandler();
  virtual ~MediaGalleryHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* values) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path, int index, void* params)
      OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Handles the "addNewGallery" message (no arguments).
  void HandleAddNewGallery(const base::ListValue* args);
  // Handles "forgetGallery" message. The first and only argument is the id of
  // the gallery.
  void HandleForgetGallery(const base::ListValue* args);

  // Called when the list of known galleries has changed; updates the page.
  void OnGalleriesChanged();

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleryHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_MEDIA_GALLERY_HANDLER_H_

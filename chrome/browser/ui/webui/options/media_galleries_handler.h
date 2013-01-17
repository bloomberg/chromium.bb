// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MEDIA_GALLERIES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MEDIA_GALLERIES_HANDLER_H_

#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace options {

// Handles messages related to adding or removing media galleries.
class MediaGalleriesHandler : public OptionsPageUIHandler,
                              public ui::SelectFileDialog::Listener {
 public:
  MediaGalleriesHandler();
  virtual ~MediaGalleriesHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* values) OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

 private:
  // Handles the "addNewGallery" message (no arguments).
  void HandleAddNewGallery(const base::ListValue* args);
  // Handles "forgetGallery" message. The first and only argument is the id of
  // the gallery.
  void HandleForgetGallery(const base::ListValue* args);

  // Called when the list of known galleries has changed; updates the page.
  void OnGalleriesChanged();

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MEDIA_GALLERIES_HANDLER_H_

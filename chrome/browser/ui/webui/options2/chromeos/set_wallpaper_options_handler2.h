// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SET_WALLPAPER_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SET_WALLPAPER_OPTIONS_HANDLER2_H_

#include "ash/desktop_background/desktop_background_resources.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

class WallpaperDelegate {
 public:
  // Call javascript function to add/update custom wallpaper thumbnail in
  // picker UI.
  virtual void SetCustomWallpaperThumbnail() = 0;

 protected:
  virtual ~WallpaperDelegate() {}
};

namespace options2 {

// ChromeOS user image options page UI handler.
class SetWallpaperOptionsHandler : public ::options2::OptionsPageUIHandler,
                                   public SelectFileDialog::Listener,
                                   public WallpaperDelegate {
 public:
  SetWallpaperOptionsHandler();
  virtual ~SetWallpaperOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // WallpaperDelegate implementation.
  virtual void SetCustomWallpaperThumbnail() OVERRIDE;

 private:
  // SelectFileDialog::Delegate implementation.
  virtual void FileSelected(const FilePath& path, int index,
                            void* params) OVERRIDE;

  // Sends list of available default images to the page.
  void SendDefaultImages();

  // Sends layout options for custom wallpaper. Only exposes CENTER,
  // CENTER_CROPPED, STRETCH to users. Set the selected option to specified
  // |layout|.
  void SendLayoutOptions(ash::WallpaperLayout layout);

  // Handles page initialized event.
  void HandlePageInitialized(const base::ListValue* args);

  // Handles page shown event.
  void HandlePageShown(const base::ListValue* args);

  // Opens a file selection dialog to choose user image from file.
  void HandleChooseFile(const base::ListValue* args);

  // Redraws the wallpaper with specified wallpaper layout.
  void HandleLayoutChanged(const base::ListValue* args);

  // Selects one of the available default wallpapers.
  void HandleDefaultWallpaper(const base::ListValue* args);

  // Sets user wallpaper to a random one from the default wallpapers.
  void HandleRandomWallpaper(const base::ListValue* args);

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  // Shows a dialog box for selecting a file.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  base::WeakPtrFactory<SetWallpaperOptionsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SetWallpaperOptionsHandler);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SET_WALLPAPER_OPTIONS_HANDLER2_H_

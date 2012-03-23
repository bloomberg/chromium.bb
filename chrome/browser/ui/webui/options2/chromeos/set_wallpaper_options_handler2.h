// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SET_WALLPAPER_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SET_WALLPAPER_OPTIONS_HANDLER2_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {
namespace options2 {

// ChromeOS user image options page UI handler.
class SetWallpaperOptionsHandler : public ::options2::OptionsPageUIHandler{
 public:
  SetWallpaperOptionsHandler();
  virtual ~SetWallpaperOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Sends list of available default images to the page.
  void SendDefaultImages();

  // Handles page initialized event.
  void HandlePageInitialized(const base::ListValue* args);

  // Handles page shown event.
  void HandlePageShown(const base::ListValue* args);

  // Selects one of the available images.
  void HandleSelectImage(const base::ListValue* args);

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  base::WeakPtrFactory<SetWallpaperOptionsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SetWallpaperOptionsHandler);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SET_WALLPAPER_OPTIONS_HANDLER2_H_

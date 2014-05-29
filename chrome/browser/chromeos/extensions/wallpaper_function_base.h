// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_

#include "ash/desktop_background/desktop_background_controller.h"
#include "extensions/browser/extension_function.h"
#include "ui/gfx/image/image_skia.h"

namespace wallpaper_api_util {
extern const char kCancelWallpaperMessage[];
ash::WallpaperLayout GetLayoutEnum(const std::string& layout);
}  // namespace wallpaper_api_util

// Wallpaper manager function base. It contains a image decoder to decode
// wallpaper data.
class WallpaperFunctionBase : public AsyncExtensionFunction {
 public:
  WallpaperFunctionBase();

 protected:
  virtual ~WallpaperFunctionBase();

  // A class to decode JPEG file.
  class UnsafeWallpaperDecoder;

  // Holds an instance of WallpaperDecoder.
  static UnsafeWallpaperDecoder* unsafe_wallpaper_decoder_;

  // Starts to decode |data|. Must run on UI thread.
  void StartDecode(const std::string& data);

  // Handles cancel case. No error message should be set.
  void OnCancel();

  // Handles failure case. Sets error message.
  void OnFailure(const std::string& error);

 private:
  virtual void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) = 0;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_

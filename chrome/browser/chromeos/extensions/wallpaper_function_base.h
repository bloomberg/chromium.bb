// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_

#include <string>
#include <vector>

#include "components/wallpaper/wallpaper_layout.h"
#include "extensions/browser/extension_function.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class SequencedTaskRunner;
}

namespace wallpaper_api_util {

extern const char kCancelWallpaperMessage[];

wallpaper::WallpaperLayout GetLayoutEnum(const std::string& layout);

// This is used to record the wallpaper layout when the user sets a custom
// wallpaper or changes the existing custom wallpaper's layout.
void RecordCustomWallpaperLayout(const wallpaper::WallpaperLayout& layout);

}  // namespace wallpaper_api_util

// Wallpaper manager function base. It contains a image decoder to decode
// wallpaper data.
class WallpaperFunctionBase : public AsyncExtensionFunction {
 public:
  WallpaperFunctionBase();

  // For tasks that are worth blocking shutdown, i.e. saving user's custom
  // wallpaper.
  static base::SequencedTaskRunner* GetBlockingTaskRunner();
  static base::SequencedTaskRunner* GetNonBlockingTaskRunner();

 protected:
  ~WallpaperFunctionBase() override;

  // A class to decode JPEG file.
  class UnsafeWallpaperDecoder;

  // Holds an instance of WallpaperDecoder.
  static UnsafeWallpaperDecoder* unsafe_wallpaper_decoder_;

  // Starts to decode |data|. Must run on UI thread.
  void StartDecode(const std::vector<char>& data);

  // Handles cancel case. No error message should be set.
  void OnCancel();

  // Handles failure case. Sets error message.
  void OnFailure(const std::string& error);

 private:
  virtual void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) = 0;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_

#include "ash/desktop_background/desktop_background_controller.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/extension_function.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/gfx/image/image_skia.h"

// Wallpaper manager strings.
class WallpaperStringsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("wallpaperPrivate.getStrings");

 protected:
  virtual ~WallpaperStringsFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Wallpaper manager function base. It contains a JPEG decoder to decode
// wallpaper data.
class WallpaperFunctionBase : public AsyncExtensionFunction {
 public:
  WallpaperFunctionBase();

 protected:
  virtual ~WallpaperFunctionBase();

  // A class to decode JPEG file.
  class WallpaperDecoder;

  // Holds an instance of WallpaperDecoder.
  static WallpaperDecoder* wallpaper_decoder_;

  // Handles failure or cancel cases. Passes error message to Javascript side.
  void OnFailureOrCancel(const std::string& error);

 private:
  virtual void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) = 0;
};

class WallpaperSetWallpaperFunction : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("wallpaperPrivate.setWallpaper");

  WallpaperSetWallpaperFunction();

 protected:
  virtual ~WallpaperSetWallpaperFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) OVERRIDE;

  // Saves the image data to a file.
  void SaveToFile();

  // Sets wallpaper to the decoded image.
  void SetDecodedWallpaper(scoped_ptr<gfx::ImageSkia> wallpaper);

  // Layout of the downloaded wallpaper.
  ash::WallpaperLayout layout_;

  // The decoded wallpaper. It may accessed from UI thread to set wallpaper or
  // FILE thread to resize and save wallpaper to disk.
  gfx::ImageSkia wallpaper_;

  // Email address of logged in user.
  std::string email_;

  // High resolution wallpaper URL.
  std::string url_;

  // String representation of downloaded wallpaper.
  std::string image_data_;

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
};

class WallpaperSetCustomWallpaperFunction : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("wallpaperPrivate.setCustomWallpaper");

  WallpaperSetCustomWallpaperFunction();

 protected:
  virtual ~WallpaperSetCustomWallpaperFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) OVERRIDE;

  // Layout of the downloaded wallpaper.
  ash::WallpaperLayout layout_;

  // Email address of logged in user.
  std::string email_;

  // String representation of downloaded wallpaper.
  std::string image_data_;
};

class WallpaperMinimizeInactiveWindowsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("wallpaperPrivate.minimizeInactiveWindows");

  WallpaperMinimizeInactiveWindowsFunction();

 protected:
  virtual ~WallpaperMinimizeInactiveWindowsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class WallpaperRestoreMinimizedWindowsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("wallpaperPrivate.restoreMinimizedWindows");

  WallpaperRestoreMinimizedWindowsFunction();

 protected:
  virtual ~WallpaperRestoreMinimizedWindowsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_

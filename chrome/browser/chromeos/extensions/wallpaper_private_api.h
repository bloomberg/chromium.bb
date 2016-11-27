// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"
#include "chrome/common/extensions/api/wallpaper_private.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class RefCountedBytes;
}

// Wallpaper manager strings.
class WallpaperPrivateGetStringsFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getStrings",
                             WALLPAPERPRIVATE_GETSTRINGS)

 protected:
  ~WallpaperPrivateGetStringsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Check if sync themes setting is enabled.
class WallpaperPrivateGetSyncSettingFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getSyncSetting",
                             WALLPAPERPRIVATE_GETSYNCSETTING)

 protected:
  ~WallpaperPrivateGetSyncSettingFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WallpaperPrivateSetWallpaperIfExistsFunction
    : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setWallpaperIfExists",
                             WALLPAPERPRIVATE_SETWALLPAPERIFEXISTS)

  WallpaperPrivateSetWallpaperIfExistsFunction();

 protected:
  ~WallpaperPrivateSetWallpaperIfExistsFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnWallpaperDecoded(const gfx::ImageSkia& image) override;

  // File doesn't exist. Sets javascript callback parameter to false.
  void OnFileNotExists(const std::string& error);

  // Reads file specified by |file_path|. If success, post a task to start
  // decoding the file.
  void ReadFileAndInitiateStartDecode(const base::FilePath& file_path,
                                      const base::FilePath& fallback_path);

  std::unique_ptr<
      extensions::api::wallpaper_private::SetWallpaperIfExists::Params>
      params;

  // User id of the active user when this api is been called.
  AccountId account_id_ = EmptyAccountId();
};

class WallpaperPrivateSetWallpaperFunction : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setWallpaper",
                             WALLPAPERPRIVATE_SETWALLPAPER)

  WallpaperPrivateSetWallpaperFunction();

 protected:
  ~WallpaperPrivateSetWallpaperFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnWallpaperDecoded(const gfx::ImageSkia& image) override;

  // Saves the image data to a file.
  void SaveToFile();

  // Sets wallpaper to the decoded image.
  void SetDecodedWallpaper(std::unique_ptr<gfx::ImageSkia> image);

  std::unique_ptr<extensions::api::wallpaper_private::SetWallpaper::Params>
      params;

  // The decoded wallpaper. It may accessed from UI thread to set wallpaper or
  // FILE thread to resize and save wallpaper to disk.
  gfx::ImageSkia wallpaper_;

  // User account id of the active user when this api is been called.
  AccountId account_id_ = EmptyAccountId();
};

class WallpaperPrivateResetWallpaperFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.resetWallpaper",
                             WALLPAPERPRIVATE_RESETWALLPAPER)

  WallpaperPrivateResetWallpaperFunction();

 protected:
  ~WallpaperPrivateResetWallpaperFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

class WallpaperPrivateSetCustomWallpaperFunction
    : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setCustomWallpaper",
                             WALLPAPERPRIVATE_SETCUSTOMWALLPAPER)

  WallpaperPrivateSetCustomWallpaperFunction();

 protected:
  ~WallpaperPrivateSetCustomWallpaperFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) override;

  // Generates thumbnail of custom wallpaper. A simple STRETCH is used for
  // generating thunbail.
  void GenerateThumbnail(const base::FilePath& thumbnail_path,
                         std::unique_ptr<gfx::ImageSkia> image);

  // Thumbnail is ready. Calls api function javascript callback.
  void ThumbnailGenerated(base::RefCountedBytes* data);

  std::unique_ptr<
      extensions::api::wallpaper_private::SetCustomWallpaper::Params>
      params;

  // User account id of the active user when this api is been called.
  AccountId account_id_ = EmptyAccountId();

  // User id hash of the logged in user.
  wallpaper::WallpaperFilesId wallpaper_files_id_;
};

class WallpaperPrivateSetCustomWallpaperLayoutFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setCustomWallpaperLayout",
                             WALLPAPERPRIVATE_SETCUSTOMWALLPAPERLAYOUT)

  WallpaperPrivateSetCustomWallpaperLayoutFunction();

 protected:
  ~WallpaperPrivateSetCustomWallpaperLayoutFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

class WallpaperPrivateMinimizeInactiveWindowsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.minimizeInactiveWindows",
                             WALLPAPERPRIVATE_MINIMIZEINACTIVEWINDOWS)

  WallpaperPrivateMinimizeInactiveWindowsFunction();

 protected:
  ~WallpaperPrivateMinimizeInactiveWindowsFunction() override;
  ResponseAction Run() override;
};

class WallpaperPrivateRestoreMinimizedWindowsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.restoreMinimizedWindows",
                             WALLPAPERPRIVATE_RESTOREMINIMIZEDWINDOWS)

  WallpaperPrivateRestoreMinimizedWindowsFunction();

 protected:
  ~WallpaperPrivateRestoreMinimizedWindowsFunction() override;
  ResponseAction Run() override;
};

class WallpaperPrivateGetThumbnailFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getThumbnail",
                             WALLPAPERPRIVATE_GETTHUMBNAIL)

  WallpaperPrivateGetThumbnailFunction();

 protected:
  ~WallpaperPrivateGetThumbnailFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  // Failed to get thumbnail for |file_name|.
  void Failure(const std::string& file_name);

  // Returns true to suppress javascript console error. Called when the
  // requested thumbnail is not found or corrupted in thumbnail directory.
  void FileNotLoaded();

  // Sets data field to the loaded thumbnail binary data in the results. Called
  // when requested wallpaper thumbnail loaded successfully.
  void FileLoaded(const std::string& data);

  // Gets thumbnail from |path|. If |path| does not exist, call FileNotLoaded().
  void Get(const base::FilePath& path);
};

class WallpaperPrivateSaveThumbnailFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.saveThumbnail",
                             WALLPAPERPRIVATE_SAVETHUMBNAIL)

  WallpaperPrivateSaveThumbnailFunction();

 protected:
  ~WallpaperPrivateSaveThumbnailFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  // Failed to save thumbnail for |file_name|.
  void Failure(const std::string& file_name);

  // Saved thumbnail to thumbnail directory.
  void Success();

  // Saves thumbnail to thumbnail directory as |file_name|.
  void Save(const std::vector<char>& data, const std::string& file_name);
};

class WallpaperPrivateGetOfflineWallpaperListFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getOfflineWallpaperList",
                             WALLPAPERPRIVATE_GETOFFLINEWALLPAPERLIST)
  WallpaperPrivateGetOfflineWallpaperListFunction();

 protected:
  ~WallpaperPrivateGetOfflineWallpaperListFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  // Enumerates the list of files in online wallpaper directory.
  void GetList();

  // Sends the list of files to extension api caller. If no files or no
  // directory, sends empty list.
  void OnComplete(const std::vector<std::string>& file_list);
};

// The wallpaper UMA is recorded when a new wallpaper is set, either by the
// built-in Wallpaper Picker App, or by a third party App.
class WallpaperPrivateRecordWallpaperUMAFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.recordWallpaperUMA",
                             WALLPAPERPRIVATE_RECORDWALLPAPERUMA)

 protected:
  ~WallpaperPrivateRecordWallpaperUMAFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_

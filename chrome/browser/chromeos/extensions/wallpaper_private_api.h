// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_

#include <string>
#include <vector>

#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"
#include "chrome/common/extensions/api/wallpaper_private.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace chromeos {
class UserImage;
}  // namespace chromeos

// Wallpaper manager strings.
class WallpaperPrivateGetStringsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getStrings",
                             WALLPAPERPRIVATE_GETSTRINGS)

 protected:
  ~WallpaperPrivateGetStringsFunction() override {}

  // SyncExtensionFunction overrides.
  bool RunSync() override;
};

// Check if sync themes setting is enabled.
class WallpaperPrivateGetSyncSettingFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getSyncSetting",
                             WALLPAPERPRIVATE_GETSYNCSETTING)

 protected:
  ~WallpaperPrivateGetSyncSettingFunction() override {}

  // SyncExtensionFunction overrides.
  bool RunSync() override;
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

  scoped_ptr<extensions::api::wallpaper_private::SetWallpaperIfExists::Params>
      params;

  // User id of the active user when this api is been called.
  std::string user_id_;

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
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
  void SetDecodedWallpaper(scoped_ptr<gfx::ImageSkia> image);

  scoped_ptr<extensions::api::wallpaper_private::SetWallpaper::Params> params;

  // The decoded wallpaper. It may accessed from UI thread to set wallpaper or
  // FILE thread to resize and save wallpaper to disk.
  gfx::ImageSkia wallpaper_;

  // User id of the active user when this api is been called.
  std::string user_id_;

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
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
                         scoped_ptr<gfx::ImageSkia> image);

  // Thumbnail is ready. Calls api function javascript callback.
  void ThumbnailGenerated(base::RefCountedBytes* data);

  scoped_ptr<extensions::api::wallpaper_private::SetCustomWallpaper::Params>
      params;

  // User id of the active user when this api is been called.
  std::string user_id_;

  // User id hash of the logged in user.
  std::string user_id_hash_;

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
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
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.minimizeInactiveWindows",
                             WALLPAPERPRIVATE_MINIMIZEINACTIVEWINDOWS)

  WallpaperPrivateMinimizeInactiveWindowsFunction();

 protected:
  ~WallpaperPrivateMinimizeInactiveWindowsFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

class WallpaperPrivateRestoreMinimizedWindowsFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.restoreMinimizedWindows",
                             WALLPAPERPRIVATE_RESTOREMINIMIZEDWINDOWS)

  WallpaperPrivateRestoreMinimizedWindowsFunction();

 protected:
  ~WallpaperPrivateRestoreMinimizedWindowsFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
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

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
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

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
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

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_

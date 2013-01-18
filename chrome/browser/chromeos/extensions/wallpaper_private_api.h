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
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getStrings",
                             WALLPAPERPRIVATE_GETSTRINGS)

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

  // Starts to decode |data|. Must run on UI thread.
  void StartDecode(const std::string& data);

  // Handles failure or cancel cases. Passes error message to Javascript side.
  void OnFailureOrCancel(const std::string& error);

 private:
  virtual void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) = 0;
};

class WallpaperSetWallpaperIfExistFunction : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setWallpaperIfExist",
                             WALLPAPERPRIVATE_SETWALLPAPERIFEXIST)

  WallpaperSetWallpaperIfExistFunction();

 protected:
  virtual ~WallpaperSetWallpaperIfExistFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) OVERRIDE;

  // Reads file specified by |file_name|. If success, post a task to start
  // decoding the file.
  void ReadFileAndInitiateStartDecode(const std::string& file_name);

  // High resolution wallpaper URL.
  std::string url_;

  // Layout of the downloaded wallpaper.
  ash::WallpaperLayout layout_;

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;

};

class WallpaperSetWallpaperFunction : public WallpaperFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setWallpaper",
                             WALLPAPERPRIVATE_SETWALLPAPER)

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
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.setCustomWallpaper",
                             WALLPAPERPRIVATE_SETCUSTOMWALLPAPER)

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
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.minimizeInactiveWindows",
                             WALLPAPERPRIVATE_MINIMIZEINACTIVEWINDOWS)

  WallpaperMinimizeInactiveWindowsFunction();

 protected:
  virtual ~WallpaperMinimizeInactiveWindowsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class WallpaperRestoreMinimizedWindowsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.restoreMinimizedWindows",
                             WALLPAPERPRIVATE_RESTOREMINIMIZEDWINDOWS)

  WallpaperRestoreMinimizedWindowsFunction();

 protected:
  virtual ~WallpaperRestoreMinimizedWindowsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class WallpaperGetThumbnailFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getThumbnail",
                             WALLPAPERPRIVATE_GETTHUMBNAIL)

  WallpaperGetThumbnailFunction();

 protected:
  virtual ~WallpaperGetThumbnailFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Failed to get thumbnail for |file_name|.
  void Failure(const std::string& file_name);

  // Returns true to suppress javascript console error. Called when the
  // requested thumbnail is not found or corrupted in thumbnail directory.
  void FileNotLoaded();

  // Sets data field to the loaded thumbnail binary data in the results. Called
  // when requested wallpaper thumbnail loaded successfully.
  void FileLoaded(const std::string& data);

  // Gets thumbnail with |file_name| from thumbnail directory. If |file_name|
  // does not exist, call FileNotLoaded().
  void Get(const std::string& file_name);

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
};

class WallpaperSaveThumbnailFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.saveThumbnail",
                             WALLPAPERPRIVATE_SAVETHUMBNAIL)

  WallpaperSaveThumbnailFunction();

 protected:
  virtual ~WallpaperSaveThumbnailFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Failed to save thumbnail for |file_name|.
  void Failure(const std::string& file_name);

  // Saved thumbnail to thumbnail directory.
  void Success();

  // Saves thumbnail to thumbnail directory as |file_name|.
  void Save(const std::string& data, const std::string& file_name);

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
};

class WallpaperGetOfflineWallpaperListFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("wallpaperPrivate.getOfflineWallpaperList",
                             WALLPAPERPRIVATE_GETOFFLINEWALLPAPERLIST)
  WallpaperGetOfflineWallpaperListFunction();

 protected:
  virtual ~WallpaperGetOfflineWallpaperListFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Enumerates the list of files in wallpaper directory.
  void GetList();

  // Sends the list of files to extension api caller. If no files or no
  // directory, sends empty list.
  void OnComplete(const std::vector<std::string>& file_list);

  // Sequence token associated with wallpaper operations. Shared with
  // WallpaperManager.
  base::SequencedWorkerPool::SequenceToken sequence_token_;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_PRIVATE_API_H_

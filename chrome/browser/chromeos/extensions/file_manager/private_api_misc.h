// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides miscellaneous API functions, which don't belong to
// other files.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_

#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/chromeos/file_manager/zip_file_creator.h"

namespace extensions {

// Implements the chrome.fileBrowserPrivate.logoutUser method.
class FileBrowserPrivateLogoutUserFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.logoutUser",
                             FILEBROWSERPRIVATE_LOGOUTUSER)

  FileBrowserPrivateLogoutUserFunction();

 protected:
  virtual ~FileBrowserPrivateLogoutUserFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getPreferences method.
// Gets settings for Files.app.
class FileBrowserPrivateGetPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getPreferences",
                             FILEBROWSERPRIVATE_GETPREFERENCES)

  FileBrowserPrivateGetPreferencesFunction();

 protected:
  virtual ~FileBrowserPrivateGetPreferencesFunction();

  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.setPreferences method.
// Sets settings for Files.app.
class FileBrowserPrivateSetPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setPreferences",
                             FILEBROWSERPRIVATE_SETPREFERENCES)

  FileBrowserPrivateSetPreferencesFunction();

 protected:
  virtual ~FileBrowserPrivateSetPreferencesFunction();

  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.zipSelection method.
// Creates a zip file for the selected files.
class FileBrowserPrivateZipSelectionFunction
    : public LoggedAsyncExtensionFunction,
      public file_manager::ZipFileCreator::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zipSelection",
                             FILEBROWSERPRIVATE_ZIPSELECTION)

  FileBrowserPrivateZipSelectionFunction();

 protected:
  virtual ~FileBrowserPrivateZipSelectionFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // extensions::ZipFileCreator::Delegate overrides.
  virtual void OnZipDone(bool success) OVERRIDE;

 private:
  scoped_refptr<file_manager::ZipFileCreator> zip_file_creator_;
};

// Implements the chrome.fileBrowserPrivate.zoom method.
// Changes the zoom level of the file manager by internally calling
// RenderViewHost::Zoom(). TODO(hirono): Remove this function once the zoom
// level change is supported for all apps. crbug.com/227175.
class FileBrowserPrivateZoomFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zoom",
                             FILEBROWSERPRIVATE_ZOOM);

  FileBrowserPrivateZoomFunction();

 protected:
  virtual ~FileBrowserPrivateZoomFunction();
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.installWebstoreItem method.
class FileBrowserPrivateInstallWebstoreItemFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.installWebstoreItem",
                             FILEBROWSERPRIVATE_INSTALLWEBSTOREITEM);

  FileBrowserPrivateInstallWebstoreItemFunction();

 protected:
  virtual ~FileBrowserPrivateInstallWebstoreItemFunction();
  virtual bool RunImpl() OVERRIDE;
  void OnInstallComplete(bool success, const std::string& error);

 private:
  std::string webstore_item_id_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_

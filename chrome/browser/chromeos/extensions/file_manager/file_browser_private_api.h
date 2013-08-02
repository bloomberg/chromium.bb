// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/chromeos/extensions/file_manager/zip_file_creator.h"
#include "chrome/browser/extensions/extension_function.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class FileManagerEventRouter;
class GURL;
class Profile;

namespace file_manager {

// Manages and registers the fileBrowserPrivate API with the extension system.
class FileBrowserPrivateAPI : public BrowserContextKeyedService {
 public:
  explicit FileBrowserPrivateAPI(Profile* profile);
  virtual ~FileBrowserPrivateAPI();

  // BrowserContextKeyedService overrides.
  virtual void Shutdown() OVERRIDE;

  // Convenience function to return the FileBrowserPrivateAPI for a Profile.
  static FileBrowserPrivateAPI* Get(Profile* profile);

  FileManagerEventRouter* event_router() {
    return event_router_.get();
  }

 private:
  scoped_ptr<FileManagerEventRouter> event_router_;
};

// Select a single file.  Closes the dialog window.
// Implements the chrome.fileBrowserPrivate.logoutUser method.
class LogoutUserFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.logoutUser",
                             FILEBROWSERPRIVATE_LOGOUTUSER)

  LogoutUserFunction();

 protected:
  virtual ~LogoutUserFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Read setting value.
class GetPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getPreferences",
                             FILEBROWSERPRIVATE_GETPREFERENCES)

  GetPreferencesFunction();

 protected:
  virtual ~GetPreferencesFunction();

  virtual bool RunImpl() OVERRIDE;
};

// Write setting value.
class SetPreferencesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setPreferences",
                             FILEBROWSERPRIVATE_SETPREFERENCES)

  SetPreferencesFunction();

 protected:
  virtual ~SetPreferencesFunction();

  virtual bool RunImpl() OVERRIDE;
};

// Create a zip file for the selected files.
class ZipSelectionFunction : public LoggedAsyncExtensionFunction,
                             public extensions::ZipFileCreator::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zipSelection",
                             FILEBROWSERPRIVATE_ZIPSELECTION)

  ZipSelectionFunction();

 protected:
  virtual ~ZipSelectionFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // extensions::ZipFileCreator::Delegate overrides.
  virtual void OnZipDone(bool success) OVERRIDE;

 private:
  scoped_refptr<extensions::ZipFileCreator> zip_file_creator_;
};

// Changes the zoom level of the file manager by internally calling
// RenderViewHost::Zoom(). TODO(hirono): Remove this function once the zoom
// level change is supported for all apps. crbug.com/227175.
class ZoomFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zoom",
                             FILEBROWSERPRIVATE_ZOOM);

  ZoomFunction();

 protected:
  virtual ~ZoomFunction();
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_

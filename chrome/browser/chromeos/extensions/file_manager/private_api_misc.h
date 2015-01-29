// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides miscellaneous API functions, which don't belong to
// other files.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_

#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "google_apis/drive/drive_api_error_codes.h"

namespace google_apis {
class AuthServiceInterface;
}

namespace extensions {

// Implements the chrome.fileManagerPrivate.logoutUserForReauthentication
// method.
class FileManagerPrivateLogoutUserForReauthenticationFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.logoutUserForReauthentication",
                             FILEMANAGERPRIVATE_LOGOUTUSERFORREAUTHENTICATION)

 protected:
  ~FileManagerPrivateLogoutUserForReauthenticationFunction() override {}

  // SyncExtensionFunction overrides.
  bool RunSync() override;
};

// Implements the chrome.fileManagerPrivate.getPreferences method.
// Gets settings for Files.app.
class FileManagerPrivateGetPreferencesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getPreferences",
                             FILEMANAGERPRIVATE_GETPREFERENCES)

 protected:
  ~FileManagerPrivateGetPreferencesFunction() override {}

  bool RunSync() override;
};

// Implements the chrome.fileManagerPrivate.setPreferences method.
// Sets settings for Files.app.
class FileManagerPrivateSetPreferencesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.setPreferences",
                             FILEMANAGERPRIVATE_SETPREFERENCES)

 protected:
  ~FileManagerPrivateSetPreferencesFunction() override {}

  bool RunSync() override;
};

// Implements the chrome.fileManagerPrivate.zipSelection method.
// Creates a zip file for the selected files.
class FileManagerPrivateZipSelectionFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.zipSelection",
                             FILEMANAGERPRIVATE_ZIPSELECTION)

  FileManagerPrivateZipSelectionFunction();

 protected:
  ~FileManagerPrivateZipSelectionFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  // Receives the result from ZipFileCreator.
  void OnZipDone(bool success);
};

// Implements the chrome.fileManagerPrivate.zoom method.
// Changes the zoom level of the file manager by internally calling
// RenderViewHost::Zoom(). TODO(hirono): Remove this function once the zoom
// level change is supported for all apps. crbug.com/227175.
class FileManagerPrivateZoomFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.zoom",
                             FILEMANAGERPRIVATE_ZOOM);

 protected:
  ~FileManagerPrivateZoomFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunSync() override;
};

// Implements the chrome.fileManagerPrivate.installWebstoreItem method.
class FileManagerPrivateInstallWebstoreItemFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.installWebstoreItem",
                             FILEMANAGERPRIVATE_INSTALLWEBSTOREITEM);

 protected:
  ~FileManagerPrivateInstallWebstoreItemFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
  void OnInstallComplete(bool success,
                         const std::string& error,
                         extensions::webstore_install::Result result);

 private:
  std::string webstore_item_id_;
};

class FileManagerPrivateRequestWebStoreAccessTokenFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.requestWebStoreAccessToken",
                             FILEMANAGERPRIVATE_REQUESTWEBSTOREACCESSTOKEN);

  FileManagerPrivateRequestWebStoreAccessTokenFunction();

 protected:
  ~FileManagerPrivateRequestWebStoreAccessTokenFunction() override;
  bool RunAsync() override;

 private:
  scoped_ptr<google_apis::AuthServiceInterface> auth_service_;

  void OnAccessTokenFetched(google_apis::DriveApiErrorCode code,
                            const std::string& access_token);

};

class FileManagerPrivateGetProfilesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getProfiles",
                             FILEMANAGERPRIVATE_GETPROFILES);

 protected:
  ~FileManagerPrivateGetProfilesFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunSync() override;
};

// Implements the chrome.fileManagerPrivate.openInspector method.
class FileManagerPrivateOpenInspectorFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.openInspector",
                             FILEMANAGERPRIVATE_OPENINSPECTOR);

 protected:
  ~FileManagerPrivateOpenInspectorFunction() override {}

  bool RunSync() override;
};

// Implements the chrome.fileManagerPrivate.getMimeType method.
class FileManagerPrivateGetMimeTypeFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getMimeType",
                             FILEMANAGERPRIVATE_GETMIMETYPE)

  FileManagerPrivateGetMimeTypeFunction();

 protected:
  ~FileManagerPrivateGetMimeTypeFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  void OnGetMimeType(const std::string& mimeType);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_

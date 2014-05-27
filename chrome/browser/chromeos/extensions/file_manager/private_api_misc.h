// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides miscellaneous API functions, which don't belong to
// other files.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_

#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace google_apis {
class AuthServiceInterface;
}

namespace extensions {

// Implements the chrome.fileBrowserPrivate.logoutUserForReauthentication
// method.
class FileBrowserPrivateLogoutUserForReauthenticationFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.logoutUserForReauthentication",
                             FILEBROWSERPRIVATE_LOGOUTUSERFORREAUTHENTICATION)

 protected:
  virtual ~FileBrowserPrivateLogoutUserForReauthenticationFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getPreferences method.
// Gets settings for Files.app.
class FileBrowserPrivateGetPreferencesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getPreferences",
                             FILEBROWSERPRIVATE_GETPREFERENCES)

 protected:
  virtual ~FileBrowserPrivateGetPreferencesFunction() {}

  virtual bool RunSync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.setPreferences method.
// Sets settings for Files.app.
class FileBrowserPrivateSetPreferencesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.setPreferences",
                             FILEBROWSERPRIVATE_SETPREFERENCES)

 protected:
  virtual ~FileBrowserPrivateSetPreferencesFunction() {}

  virtual bool RunSync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.zipSelection method.
// Creates a zip file for the selected files.
class FileBrowserPrivateZipSelectionFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zipSelection",
                             FILEBROWSERPRIVATE_ZIPSELECTION)

  FileBrowserPrivateZipSelectionFunction();

 protected:
  virtual ~FileBrowserPrivateZipSelectionFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  // Receives the result from ZipFileCreator.
  void OnZipDone(bool success);
};

// Implements the chrome.fileBrowserPrivate.zoom method.
// Changes the zoom level of the file manager by internally calling
// RenderViewHost::Zoom(). TODO(hirono): Remove this function once the zoom
// level change is supported for all apps. crbug.com/227175.
class FileBrowserPrivateZoomFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.zoom",
                             FILEBROWSERPRIVATE_ZOOM);

 protected:
  virtual ~FileBrowserPrivateZoomFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.installWebstoreItem method.
class FileBrowserPrivateInstallWebstoreItemFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.installWebstoreItem",
                             FILEBROWSERPRIVATE_INSTALLWEBSTOREITEM);

 protected:
  virtual ~FileBrowserPrivateInstallWebstoreItemFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
  void OnInstallComplete(bool success, const std::string& error);

 private:
  std::string webstore_item_id_;
};

class FileBrowserPrivateRequestWebStoreAccessTokenFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestWebStoreAccessToken",
                             FILEBROWSERPRIVATE_REQUESTWEBSTOREACCESSTOKEN);

  FileBrowserPrivateRequestWebStoreAccessTokenFunction();

 protected:
  virtual ~FileBrowserPrivateRequestWebStoreAccessTokenFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  scoped_ptr<google_apis::AuthServiceInterface> auth_service_;

  void OnAccessTokenFetched(google_apis::GDataErrorCode code,
                            const std::string& access_token);

};

class FileBrowserPrivateGetProfilesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getProfiles",
                             FILEBROWSERPRIVATE_GETPROFILES);

 protected:
  virtual ~FileBrowserPrivateGetProfilesFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

class FileBrowserPrivateVisitDesktopFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.visitDesktop",
                             FILEBROWSERPRIVATE_VISITDESKTOP);

 protected:
  virtual ~FileBrowserPrivateVisitDesktopFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.openInspector method.
class FileBrowserPrivateOpenInspectorFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.openInspector",
                             FILEBROWSERPRIVATE_OPENINSPECTOR);

 protected:
  virtual ~FileBrowserPrivateOpenInspectorFunction() {}

  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_

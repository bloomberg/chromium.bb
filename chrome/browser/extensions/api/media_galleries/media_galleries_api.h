// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Media Galleries API functions for accessing
// user's media files, as specified in the extension API IDL.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_

#include <vector>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"

namespace extensions {

class MediaGalleriesGetMediaFileSystemsFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.getMediaFileSystems",
                             MEDIAGALLERIES_GETMEDIAFILESYSTEMS)

 protected:
  virtual ~MediaGalleriesGetMediaFileSystemsFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  // Always show the dialog.
  void AlwaysShowDialog(
      const std::vector<chrome::MediaFileSystemInfo>& filesystems);

  // If no galleries are found, show the dialog, otherwise return them.
  void ShowDialogIfNoGalleries(
      const std::vector<chrome::MediaFileSystemInfo>& filesystems);

  // Grabs galleries from the media file system registry and passes them to
  // |ReturnGalleries|.
  void GetAndReturnGalleries();

  // Returns galleries to the caller.
  void ReturnGalleries(
      const std::vector<chrome::MediaFileSystemInfo>& filesystems);

  // Shows the configuration dialog to edit gallery preferences.
  void ShowDialog();

  // A helper method that calls
  // MediaFileSystemRegistry::GetMediaFileSystemsForExtension().
  void GetMediaFileSystemsForExtension(
      const chrome::MediaFileSystemsCallback& cb);
};

class MediaGalleriesAssembleMediaFileFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "experimental.mediaGalleries.assembleMediaFile",
      EXPERIMENTAL_MEDIAGALLERIES_ASSEMBLEMEDIAFILE)

 protected:
  virtual ~MediaGalleriesAssembleMediaFileFunction();
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_

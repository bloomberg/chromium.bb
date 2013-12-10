// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Media Galleries API functions for accessing
// user's media files, as specified in the extension API IDL.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/common/extensions/api/media_galleries.h"

namespace MediaGalleries = extensions::api::media_galleries;

namespace extensions {

class MediaGalleriesGetMediaFileSystemsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.getMediaFileSystems",
                             MEDIAGALLERIES_GETMEDIAFILESYSTEMS)

 protected:
  virtual ~MediaGalleriesGetMediaFileSystemsFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  // Bottom half for RunImpl, invoked after the preferences is initialized.
  void OnPreferencesInit(
    MediaGalleries::GetMediaFileSystemsInteractivity interactive);

  // Always show the dialog.
  void AlwaysShowDialog(const std::vector<MediaFileSystemInfo>& filesystems);

  // If no galleries are found, show the dialog, otherwise return them.
  void ShowDialogIfNoGalleries(
      const std::vector<MediaFileSystemInfo>& filesystems);

  // Grabs galleries from the media file system registry and passes them to
  // |ReturnGalleries|.
  void GetAndReturnGalleries();

  // Returns galleries to the caller.
  void ReturnGalleries(const std::vector<MediaFileSystemInfo>& filesystems);

  // Shows the configuration dialog to edit gallery preferences.
  void ShowDialog();

  // A helper method that calls
  // MediaFileSystemRegistry::GetMediaFileSystemsForExtension().
  void GetMediaFileSystemsForExtension(const MediaFileSystemsCallback& cb);
};

class MediaGalleriesGetAllMediaFileSystemMetadataFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.getAllMediaFileSystemMetadata",
                             MEDIAGALLERIES_GETALLMEDIAFILESYSTEMMETADATA)

 protected:
  virtual ~MediaGalleriesGetAllMediaFileSystemMetadataFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  // Bottom half for RunImpl, invoked after the preferences is initialized.
  // Gets the list of permitted galleries and checks if they are available.
  void OnPreferencesInit();

  // Callback to run upon getting the list of available devices.
  // Sends the list of media filesystem metadata back to the extension.
  void OnGetGalleries(const MediaGalleryPrefIdSet& permitted_gallery_ids,
                      const MediaStorageUtil::DeviceIdSet* available_devices);
};

class MediaGalleriesGetMetadataFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.getMetadata",
                             MEDIAGALLERIES_GETMETADATA)

 protected:
  virtual ~MediaGalleriesGetMetadataFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  // Bottom half for RunImpl, invoked after the preferences is initialized.
  void OnPreferencesInit(bool mime_type_only, const std::string& blob_uuid);

  void SniffMimeType(bool mime_type_only, scoped_ptr<std::string> blob_header);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Media Galleries API functions for accessing
// user's media files, as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERY_MEDIA_GALLERY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERY_MEDIA_GALLERY_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class GetMediaFileSystemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.mediaGalleries.getMediaFileSystems")

 protected:
  virtual ~GetMediaFileSystemsFunction();
  virtual bool RunImpl() OVERRIDE;
};

class OpenMediaGalleryManagerFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.mediaGalleries.openMediaGalleryManager")

 protected:
  virtual ~OpenMediaGalleryManagerFunction();
  virtual bool RunImpl() OVERRIDE;
};

class AssembleMediaFileFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.mediaGalleries.assembleMediaFile")

 protected:
  virtual ~AssembleMediaFileFunction();
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERY_MEDIA_GALLERY_API_H_

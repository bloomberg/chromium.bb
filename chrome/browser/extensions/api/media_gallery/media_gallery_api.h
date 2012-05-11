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
  virtual ~GetMediaFileSystemsFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.mediaGalleries.getMediaFileSystems")
};

class OpenMediaGalleryManagerFunction : public SyncExtensionFunction {
 public:
  virtual ~OpenMediaGalleryManagerFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.mediaGalleries.openMediaGalleryManager")
};

class AssembleMediaFileFunction : public SyncExtensionFunction {
 public:
  virtual ~AssembleMediaFileFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.mediaGalleries.assembleMediaFile")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERY_MEDIA_GALLERY_API_H_

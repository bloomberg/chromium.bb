// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Cookies API.

#include "chrome/browser/extensions/api/media_gallery/media_gallery_api.h"

#include "base/values.h"

namespace extensions {

GetMediaGalleriesFunction::~GetMediaGalleriesFunction() {}

bool GetMediaGalleriesFunction::RunImpl() {
  // TODO(vandebo) Get the list of galleries.
  result_.reset(new ListValue);
  return true;
}

OpenMediaGalleryManagerFunction::~OpenMediaGalleryManagerFunction() {}

bool OpenMediaGalleryManagerFunction::RunImpl() {
  // TODO(vandebo) Open a new tab/ui surface with config UI.
  return true;
}

AssembleMediaFileFunction::~AssembleMediaFileFunction() {}

bool AssembleMediaFileFunction::RunImpl() {
  // TODO(vandebo) Update the metadata and return the new file.
  result_.reset(Value::CreateNullValue());
  return true;
}

ParseMediaFileMetadataFunction::~ParseMediaFileMetadataFunction() {}

bool ParseMediaFileMetadataFunction::RunImpl() {
  // TODO(vandebo) Try to parse the file.
  result_.reset(Value::CreateNullValue());
  return true;
}

}  // namespace extensions

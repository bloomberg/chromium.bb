// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Media Galleries API.

#include "chrome/browser/extensions/api/media_gallery/media_gallery_api.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace extensions {

GetMediaFileSystemsFunction::~GetMediaFileSystemsFunction() {}

bool GetMediaFileSystemsFunction::RunImpl() {
  // TODO(vandebo) Get the list of galleries.
  result_.reset(new ListValue());
  return true;
}

OpenMediaGalleryManagerFunction::~OpenMediaGalleryManagerFunction() {}

bool OpenMediaGalleryManagerFunction::RunImpl() {
  // TODO(vandebo) Open the Media Gallery Manager UI.
  result_.reset(Value::CreateNullValue());
  return true;
}

AssembleMediaFileFunction::~AssembleMediaFileFunction() {}

bool AssembleMediaFileFunction::RunImpl() {
  // TODO(vandebo) Update the metadata and return the new file.
  result_.reset(Value::CreateNullValue());
  return true;
}

}  // namespace extensions

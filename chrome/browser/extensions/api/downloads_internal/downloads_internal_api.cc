// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/downloads_internal/downloads_internal_api.h"

#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/common/extensions/api/downloads_internal.h"

DownloadsInternalDetermineFilenameFunction::
    DownloadsInternalDetermineFilenameFunction() {}

DownloadsInternalDetermineFilenameFunction::
    ~DownloadsInternalDetermineFilenameFunction() {}

typedef extensions::api::downloads_internal::DetermineFilename::Params
    DetermineFilenameParams;

bool DownloadsInternalDetermineFilenameFunction::RunImpl() {
  scoped_ptr<DetermineFilenameParams> params(
      DetermineFilenameParams::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::FilePath::StringType filename;
  if (params->filename.get()) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filename));
  }
  return ExtensionDownloadsEventRouter::DetermineFilename(
      profile(),
      include_incognito(),
      GetExtension()->id(),
      params->download_id,
      base::FilePath(filename),
      params->overwrite && *params->overwrite,
      &error_);
}

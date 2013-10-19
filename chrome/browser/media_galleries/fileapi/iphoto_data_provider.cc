// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iphoto_data_provider.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/safe_iapps_library_parser.h"
#include "content/public/browser/browser_thread.h"

namespace iphoto {

IPhotoDataProvider::IPhotoDataProvider(const base::FilePath& library_path)
    : iapps::IAppsDataProvider(library_path),
      weak_factory_(this) {}

IPhotoDataProvider::~IPhotoDataProvider() {}

void IPhotoDataProvider::DoParseLibrary(
    const base::FilePath& library_path,
    const ReadyCallback& ready_callback) {
  xml_parser_ = new iapps::SafeIAppsLibraryParser;
  xml_parser_->ParseIPhotoLibrary(
      library_path,
      base::Bind(&IPhotoDataProvider::OnLibraryParsed,
                 weak_factory_.GetWeakPtr(),
                 ready_callback));
}

void IPhotoDataProvider::OnLibraryParsed(const ReadyCallback& ready_callback,
                                         bool result,
                                         const parser::Library& library) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  set_valid(result);
  if (valid()) {
    library_ = library;
  }
  ready_callback.Run(valid());
}

}  // namespace iphoto

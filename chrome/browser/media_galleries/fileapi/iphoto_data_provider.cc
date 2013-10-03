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
#include "chrome/browser/media_galleries/fileapi/file_path_watcher_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "content/public/browser/browser_thread.h"

namespace iphoto {

IPhotoDataProvider::IPhotoDataProvider(const base::FilePath& library_path)
    : iapps::IAppsDataProvider(library_path) {}

IPhotoDataProvider::~IPhotoDataProvider() {}

void IPhotoDataProvider::DoParseLibrary(
    const base::FilePath& library_path,
    const ReadyCallback& ready_callback) {
  set_valid(true);
  ready_callback.Run(true);
}
}  // namespace iphoto

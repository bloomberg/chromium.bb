// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iapps_data_provider.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/media_galleries/fileapi/file_path_watcher_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/media_galleries/itunes_library.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "webkit/browser/fileapi/native_file_util.h"

namespace iapps {

IAppsDataProvider::IAppsDataProvider(const base::FilePath& library_path)
    : library_path_(library_path),
      needs_refresh_(true),
      is_valid_(false),
      weak_factory_(this) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!library_path_.empty());

  StartFilePathWatchOnMediaTaskRunner(
      library_path_,
      base::Bind(&IAppsDataProvider::OnLibraryWatchStarted,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&IAppsDataProvider::OnLibraryChanged,
                 weak_factory_.GetWeakPtr()));
}

IAppsDataProvider::~IAppsDataProvider() {}

bool IAppsDataProvider::valid() const {
  return is_valid_;
}

void IAppsDataProvider::set_valid(bool valid) {
  is_valid_ = valid;
}

void IAppsDataProvider::RefreshData(const ReadyCallback& ready_callback) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  if (!needs_refresh_) {
    ready_callback.Run(valid());
    return;
  }

  // TODO(gbillock): this needs re-examination. Could be a refresh bug.
  needs_refresh_ = false;
  DoParseLibrary(library_path_, ready_callback);
}

const base::FilePath& IAppsDataProvider::library_path() const {
  return library_path_;
}

void IAppsDataProvider::OnLibraryWatchStarted(
    scoped_ptr<base::FilePathWatcher> library_watcher) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  library_watcher_.reset(library_watcher.release());
}

void IAppsDataProvider::OnLibraryChanged(const base::FilePath& path,
                                         bool error) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK_EQ(library_path_.value(), path.value());
  if (error)
    LOG(ERROR) << "Error watching " << library_path_.value();
  needs_refresh_ = true;
}

}  // namespace iapps

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa_finder.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "content/public/browser/browser_thread.h"

namespace picasa {

namespace {

// Returns path of Picasa's DB3 database directory. May only be called on
// threads that allow for disk IO, like the FILE thread or MediaTaskRunner.
base::FilePath FindPicasaDatabaseOnFileThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  base::FilePath path;

#if defined(OS_WIN)
  // TODO(tommycli): Check registry for alternative path.
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, &path))
    return base::FilePath();
#elif defined(OS_MACOSX)
  // TODO(tommycli): Check Mac Preferences for alternative path.
  if (!PathService::Get(base::DIR_APP_DATA, &path))
    return base::FilePath();
#else
  return base::FilePath();
#endif

  path = path.AppendASCII("Google").AppendASCII("Picasa2")
             .AppendASCII(kPicasaDatabaseDirName);

  // Verify actual existence
  if (!base::DirectoryExists(path))
    path.clear();

  return path;
}

void FinishOnOriginalThread(const PicasaFinder::DeviceIDCallback& callback,
                            const base::FilePath& database_path) {
  if (!database_path.empty())
    callback.Run(StorageInfo::MakeDeviceId(StorageInfo::PICASA,
                                           database_path.AsUTF8Unsafe()));
}

}  // namespace

void PicasaFinder::FindPicasaDatabase(
    const PicasaFinder::DeviceIDCallback& callback) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&FindPicasaDatabaseOnFileThread),
      base::Bind(&FinishOnOriginalThread, callback));
}

}  // namespace picasa

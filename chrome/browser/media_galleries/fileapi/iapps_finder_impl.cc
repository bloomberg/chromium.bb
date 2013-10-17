// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iapps_finder_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

namespace iapps {

namespace {

void PostResultToUIThread(StorageInfo::Type type,
                          const IAppsFinderCallback& callback,
                          const std::string& unique_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string device_id;
  if (!unique_id.empty())
    device_id = StorageInfo::MakeDeviceId(type, unique_id);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(callback, device_id));
}

}  // namespace

void FindIAppsOnFileThread(StorageInfo::Type type, const IAppsFinderTask& task,
                           const IAppsFinderCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!task.is_null());
  DCHECK(!callback.is_null());

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(task, base::Bind(PostResultToUIThread, type, callback)));
}

// iPhoto is only supported on OSX.
#if !defined(OS_MACOSX)
void FindIPhotoLibrary(const IAppsFinderCallback& callback) {
  callback.Run(std::string());
}
#endif  // !defined(OS_MACOSX)

// iTunes is only support on OSX and Windows.
#if !defined(OS_MACOSX) && !defined(OS_WIN)
void FindITunesLibrary(const IAppsFinderCallback& callback) {
  callback.Run(std::string());
}
#endif

}  // namespace iapps

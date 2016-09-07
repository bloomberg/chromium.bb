// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list_android.h"

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/threading/sequenced_worker_pool.h"

CrashUploadListAndroid::CrashUploadListAndroid(
    Delegate* delegate,
    const base::FilePath& upload_log_path,
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool)
    : CrashUploadList(delegate, upload_log_path, worker_pool) {}

CrashUploadListAndroid::~CrashUploadListAndroid() {}

void CrashUploadListAndroid::LoadUploadList(
    std::vector<UploadList::UploadInfo>* uploads) {
  // This will load the list of successfully uploaded logs.
  CrashUploadList::LoadUploadList(uploads);

  LoadUnsuccessfulUploadList(uploads);
}

void CrashUploadListAndroid::LoadUnsuccessfulUploadList(
    std::vector<UploadInfo>* uploads) {
  const char unsuccessful_uploads[] = ".dmp";
  const char skipped_uploads[] = ".skipped";

  base::FileEnumerator files(upload_log_path().DirName(), false,
                             base::FileEnumerator::FILES);
  for (base::FilePath file = files.Next(); !file.empty(); file = files.Next()) {
    if (file.value().find(unsuccessful_uploads) == std::string::npos &&
        file.value().find(skipped_uploads) == std::string::npos)
      continue;

    base::File::Info info;
    if (!base::GetFileInfo(file, &info))
      continue;

    // Crash reports can have multiple extensions (e.g. foo.dmp, foo.dmp.try1,
    // foo.skipped.try0).
    file = file.BaseName();
    while (file != file.RemoveExtension())
      file = file.RemoveExtension();

    // ID is the last part of the file name. e.g.
    // chromium-renderer-minidump-f297dbcba7a2d0bb.
    std::string id = file.value();
    std::size_t pos = id.find_last_of("-");
    if (pos == std::string::npos)
      continue;

    id = id.substr(pos + 1);
    UploadList::UploadInfo upload(std::string(), base::Time(), id,
                                  info.creation_time,
                                  UploadList::UploadInfo::State::NotUploaded);
    uploads->push_back(upload);
  }
}

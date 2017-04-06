// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list_android.h"

#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/task_runner.h"
#include "jni/MinidumpUploadService_jni.h"
#include "ui/base/text/bytes_formatting.h"

CrashUploadListAndroid::CrashUploadListAndroid(
    Delegate* delegate,
    const base::FilePath& upload_log_path,
    scoped_refptr<base::TaskRunner> task_runner)
    : CrashUploadList(delegate, upload_log_path, std::move(task_runner)) {}

CrashUploadListAndroid::~CrashUploadListAndroid() {}

void CrashUploadListAndroid::LoadUploadList(
    std::vector<UploadList::UploadInfo>* uploads) {
  // This will load the list of successfully uploaded logs.
  CrashUploadList::LoadUploadList(uploads);

  LoadUnsuccessfulUploadList(uploads);
}

void CrashUploadListAndroid::RequestSingleCrashUpload(
    const std::string& local_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const base::android::JavaRef<jobject>& context =
      base::android::GetApplicationContext();
  base::android::ScopedJavaLocalRef<jstring> j_local_id =
      base::android::ConvertUTF8ToJavaString(env, local_id);
  Java_MinidumpUploadService_tryUploadCrashDumpWithLocalId(env, context,
                                                           j_local_id);
}

void CrashUploadListAndroid::LoadUnsuccessfulUploadList(
    std::vector<UploadInfo>* uploads) {
  const char pending_uploads[] = ".dmp";
  const char skipped_uploads[] = ".skipped";
  const char manually_forced_uploads[] = ".forced";

  base::FileEnumerator files(upload_log_path().DirName(), false,
                             base::FileEnumerator::FILES);
  for (base::FilePath file = files.Next(); !file.empty(); file = files.Next()) {
    UploadList::UploadInfo::State upload_state;
    if (file.value().find(manually_forced_uploads) != std::string::npos) {
      upload_state = UploadList::UploadInfo::State::Pending_UserRequested;
    } else if (file.value().find(pending_uploads) != std::string::npos) {
      upload_state = UploadList::UploadInfo::State::Pending;
    } else if (file.value().find(skipped_uploads) != std::string::npos) {
      upload_state = UploadList::UploadInfo::State::NotUploaded;
    } else {
      // The |file| is something other than a minidump file, e.g. a logcat file.
      continue;
    }

    base::File::Info info;
    if (!base::GetFileInfo(file, &info))
      continue;

    int64_t file_size = 0;
    if (!base::GetFileSize(file, &file_size))
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
    UploadList::UploadInfo upload(id, info.creation_time, upload_state,
                                  ui::FormatBytes(file_size));
    uploads->push_back(upload);
  }
}

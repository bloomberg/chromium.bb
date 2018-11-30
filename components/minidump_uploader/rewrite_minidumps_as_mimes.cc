// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "jni/CrashReportMimeWriter_jni.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/handler/minidump_to_upload_parameters.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/util/file/file_writer.h"
#include "third_party/crashpad/crashpad/util/net/http_body.h"
#include "third_party/crashpad/crashpad/util/net/http_multipart_builder.h"

namespace minidump_uploader {

namespace {

#if defined(OS_ANDROID)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProcessedMinidumpCounts {
  kOther = 0,
  kBrowser = 1,
  kRenderer = 2,
  kGpu = 3,
  kMaxValue = kGpu
};
#endif  // OS_ANDROID

bool MimeifyReport(const crashpad::CrashReportDatabase::UploadReport* report,
                   const base::FilePath& dest_dir) {
  crashpad::FileReader* reader = report->Reader();
  crashpad::FileOffset start_offset = reader->SeekGet();
  if (start_offset < 0) {
    return false;
  }

  // Ignore any errors that might occur when attempting to interpret the
  // minidump file. This may result in its being uploaded with few or no
  // parameters, but as long as there’s a dump file, the server can decide what
  // to do with it.
  std::map<std::string, std::string> parameters;
  crashpad::ProcessSnapshotMinidump minidump_process_snapshot;
  if (minidump_process_snapshot.Initialize(reader)) {
    parameters =
        BreakpadHTTPFormParametersFromMinidump(&minidump_process_snapshot);
  }

  if (!reader->SeekSet(start_offset)) {
    return false;
  }

  crashpad::HTTPMultipartBuilder http_multipart_builder;

  static constexpr char kMinidumpKey[] = "upload_file_minidump";
  static constexpr char kPtypeKey[] = "ptype";

  for (const auto& kv : parameters) {
    if (kv.first == kMinidumpKey) {
      LOG(WARNING) << "reserved key " << kv.first << ", discarding value "
                   << kv.second;
    } else {
      http_multipart_builder.SetFormData(kv.first, kv.second);
#if defined(OS_ANDROID)
      if (kv.first == kPtypeKey) {
        ProcessedMinidumpCounts count_type;
        if (kv.second == "browser") {
          count_type = ProcessedMinidumpCounts::kBrowser;
        } else if (kv.second == "renderer") {
          count_type = ProcessedMinidumpCounts::kRenderer;
        } else if (kv.second == "gpu-process") {
          count_type = ProcessedMinidumpCounts::kGpu;
        } else {
          count_type = ProcessedMinidumpCounts::kOther;
        }
        UMA_HISTOGRAM_ENUMERATION("Stability.Android.ProcessedMinidumps",
                                  count_type);
      }
#endif  // OS_ANDROID
    }
  }

  http_multipart_builder.SetFileAttachment(kMinidumpKey,
                                           report->uuid.ToString() + ".dmp",
                                           reader, "application/octet-stream");

  std::unique_ptr<crashpad::HTTPBodyStream> body =
      http_multipart_builder.GetBodyStream();
  crashpad::FileWriter writer;
  if (!writer.Open(dest_dir.Append(base::StringPrintf(
                       "%s.dmp%d", report->uuid.ToString().c_str(),
                       minidump_process_snapshot.ProcessID())),
                   crashpad::FileWriteMode::kCreateOrFail,
                   crashpad::FilePermissions::kOwnerOnly)) {
    return false;
  }

  uint8_t buffer[4096];
  crashpad::FileOperationResult bytes_read;
  while ((bytes_read = body->GetBytesBuffer(buffer, sizeof(buffer))) > 0) {
    writer.Write(buffer, bytes_read);
  }
  return bytes_read == 0;
}

}  // namespace

void RewriteMinidumpsAsMIMEs(const base::FilePath& src_dir,
                             const base::FilePath& dest_dir) {
  std::unique_ptr<crashpad::CrashReportDatabase> db =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(src_dir);
  if (!db) {
    return;
  }

  std::vector<crashpad::CrashReportDatabase::Report> reports;
  if (db->GetPendingReports(&reports) !=
      crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  for (const auto& report : reports) {
    std::unique_ptr<const crashpad::CrashReportDatabase::UploadReport>
        upload_report;
    switch (db->GetReportForUploading(report.uuid,
                                      &upload_report,
                                      /* report_metrics= */ false)) {
      case crashpad::CrashReportDatabase::kBusyError:
      case crashpad::CrashReportDatabase::kReportNotFound:
        continue;

      case crashpad::CrashReportDatabase::kNoError:
        if (MimeifyReport(upload_report.get(), dest_dir)) {
          db->RecordUploadComplete(std::move(upload_report), std::string());
        } else {
          crashpad::Metrics::CrashUploadSkipped(
              crashpad::Metrics::CrashSkippedReason::kPrepareForUploadFailed);
          upload_report.reset();
        }
        db->DeleteReport(report.uuid);
        continue;

      case crashpad::CrashReportDatabase::kFileSystemError:
      case crashpad::CrashReportDatabase::kDatabaseError:
        crashpad::Metrics::CrashUploadSkipped(
            crashpad::Metrics::CrashSkippedReason::kDatabaseError);
        db->DeleteReport(report.uuid);
        continue;

      case crashpad::CrashReportDatabase::kCannotRequestUpload:
        NOTREACHED();
        db->DeleteReport(report.uuid);
        continue;
    }
  }
}

static void JNI_CrashReportMimeWriter_RewriteMinidumpsAsMIMEs(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_src_dir,
    const base::android::JavaParamRef<jstring>& j_dest_dir) {
  std::string src_dir, dest_dir;
  base::android::ConvertJavaStringToUTF8(env, j_src_dir, &src_dir);
  base::android::ConvertJavaStringToUTF8(env, j_dest_dir, &dest_dir);

  RewriteMinidumpsAsMIMEs(base::FilePath(src_dir), base::FilePath(dest_dir));
}

}  // namespace minidump_uploader

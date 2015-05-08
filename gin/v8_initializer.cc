// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_initializer.h"

#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "crypto/sha2.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif  // OS_MACOSX
#include "base/path_service.h"
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

namespace gin {

namespace {

base::MemoryMappedFile* g_mapped_natives = nullptr;
base::MemoryMappedFile* g_mapped_snapshot = nullptr;

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if !defined(OS_MACOSX)
const int kV8SnapshotBasePathKey =
#if defined(OS_ANDROID)
    base::DIR_ANDROID_APP_DATA;
#elif defined(OS_POSIX)
    base::DIR_EXE;
#elif defined(OS_WIN)
    base::DIR_MODULE;
#endif  // OS_ANDROID
#endif  // !OS_MACOSX

const char kNativesFileName[] = "natives_blob.bin";
const char kSnapshotFileName[] = "snapshot_blob.bin";

// Constants for snapshot loading retries taken from:
//   https://support.microsoft.com/en-us/kb/316609.
const int kMaxOpenAttempts = 5;
const int kOpenRetryDelayMillis = 250;

void GetV8FilePaths(base::FilePath* natives_path_out,
                    base::FilePath* snapshot_path_out) {
#if !defined(OS_MACOSX)
  base::FilePath data_path;
  PathService::Get(kV8SnapshotBasePathKey, &data_path);
  DCHECK(!data_path.empty());

  *natives_path_out = data_path.AppendASCII(kNativesFileName);
  *snapshot_path_out = data_path.AppendASCII(kSnapshotFileName);
#else   // !defined(OS_MACOSX)
  base::ScopedCFTypeRef<CFStringRef> natives_file_name(
      base::SysUTF8ToCFStringRef(kNativesFileName));
  *natives_path_out =
      base::mac::PathForFrameworkBundleResource(natives_file_name);
  base::ScopedCFTypeRef<CFStringRef> snapshot_file_name(
      base::SysUTF8ToCFStringRef(kSnapshotFileName));
  *snapshot_path_out =
      base::mac::PathForFrameworkBundleResource(snapshot_file_name);
  DCHECK(!natives_path_out->empty());
  DCHECK(!snapshot_path_out->empty());
#endif  // !defined(OS_MACOSX)
}

static bool MapV8Files(base::File natives_file,
                       base::File snapshot_file,
                       base::MemoryMappedFile::Region natives_region =
                           base::MemoryMappedFile::Region::kWholeFile,
                       base::MemoryMappedFile::Region snapshot_region =
                           base::MemoryMappedFile::Region::kWholeFile) {
  g_mapped_natives = new base::MemoryMappedFile;
  if (!g_mapped_natives->IsValid()) {
    if (!g_mapped_natives->Initialize(natives_file.Pass(), natives_region)) {
      delete g_mapped_natives;
      g_mapped_natives = NULL;
      LOG(FATAL) << "Couldn't mmap v8 natives data file";
      return false;
    }
  }

  g_mapped_snapshot = new base::MemoryMappedFile;
  if (!g_mapped_snapshot->IsValid()) {
    if (!g_mapped_snapshot->Initialize(snapshot_file.Pass(), snapshot_region)) {
      delete g_mapped_snapshot;
      g_mapped_snapshot = NULL;
      LOG(ERROR) << "Couldn't mmap v8 snapshot data file";
      return false;
    }
  }

  return true;
}

static bool OpenV8File(const base::FilePath& path,
                       int flags,
                       base::File& file) {
  // Re-try logic here is motivated by http://crbug.com/479537
  // for A/V on Windows (https://support.microsoft.com/en-us/kb/316609).

  // These match tools/metrics/histograms.xml
  enum OpenV8FileResult {
    OPENED = 0,
    OPENED_RETRY,
    FAILED_IN_USE,
    FAILED_OTHER,
    MAX_VALUE
  };

  OpenV8FileResult result = OpenV8FileResult::FAILED_IN_USE;
  for (int attempt = 0; attempt < kMaxOpenAttempts; attempt++) {
    file.Initialize(path, flags);
    if (file.IsValid()) {
      if (attempt == 0) {
        result = OpenV8FileResult::OPENED;
        break;
      } else {
        result = OpenV8FileResult::OPENED_RETRY;
        break;
      }
    } else if (file.error_details() != base::File::FILE_ERROR_IN_USE) {
      result = OpenV8FileResult::FAILED_OTHER;
      break;
    } else if (kMaxOpenAttempts - 1 != attempt) {
      base::PlatformThread::Sleep(
          base::TimeDelta::FromMilliseconds(kOpenRetryDelayMillis));
    }
  }

  UMA_HISTOGRAM_ENUMERATION("V8.Initializer.OpenV8File.Result",
                            result,
                            OpenV8FileResult::MAX_VALUE);

  return result == OpenV8FileResult::OPENED
         || result == OpenV8FileResult::OPENED_RETRY;
}

#if defined(V8_VERIFY_EXTERNAL_STARTUP_DATA)
bool VerifyV8SnapshotFile(base::MemoryMappedFile* snapshot_file,
                          const unsigned char* fingerprint) {
  unsigned char output[crypto::kSHA256Length];
  crypto::SHA256HashString(
      base::StringPiece(reinterpret_cast<const char*>(snapshot_file->data()),
                        snapshot_file->length()),
      output, sizeof(output));
  return !memcmp(fingerprint, output, sizeof(output));
}
#endif  // V8_VERIFY_EXTERNAL_STARTUP_DATA
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  base::RandBytes(buffer, amount);
  return true;
}

}  // namespace

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(V8_VERIFY_EXTERNAL_STARTUP_DATA)
// Defined in gen/gin/v8_snapshot_fingerprint.cc
extern const unsigned char g_natives_fingerprint[];
extern const unsigned char g_snapshot_fingerprint[];
#endif  // V8_VERIFY_EXTERNAL_STARTUP_DATA

// static
bool V8Initializer::LoadV8Snapshot() {

  enum LoadV8SnapshotResult {
    SUCCESS = 0,
    FAILED_OPEN,
    FAILED_MAP,
    FAILED_VERIFY,
    MAX_VALUE
  };

  if (g_mapped_natives && g_mapped_snapshot)
    return true;

  base::FilePath natives_data_path;
  base::FilePath snapshot_data_path;
  GetV8FilePaths(&natives_data_path, &snapshot_data_path);

  base::File natives_file;
  base::File snapshot_file;
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;

  LoadV8SnapshotResult result;
  if (!OpenV8File(natives_data_path, flags, natives_file) ||
      !OpenV8File(snapshot_data_path, flags, snapshot_file)) {
    result = LoadV8SnapshotResult::FAILED_OPEN;
  } else if (!MapV8Files(natives_file.Pass(), snapshot_file.Pass())) {
    result = LoadV8SnapshotResult::FAILED_MAP;
#if defined(V8_VERIFY_EXTERNAL_STARTUP_DATA)
  } else if (!VerifyV8SnapshotFile(g_mapped_natives, g_natives_fingerprint) ||
             !VerifyV8SnapshotFile(g_mapped_snapshot, g_snapshot_fingerprint)) {
    result = LoadV8SnapshotResult::FAILED_VERIFY;
#endif  // V8_VERIFY_EXTERNAL_STARTUP_DATA
  } else {
    result = LoadV8SnapshotResult::SUCCESS;
  }

  UMA_HISTOGRAM_ENUMERATION("V8.Initializer.LoadV8Snapshot.Result",
                            result,
                            LoadV8SnapshotResult::MAX_VALUE);
  return result == LoadV8SnapshotResult::SUCCESS;
}

// static
bool V8Initializer::LoadV8SnapshotFromFD(base::PlatformFile natives_pf,
                                         int64 natives_offset,
                                         int64 natives_size,
                                         base::PlatformFile snapshot_pf,
                                         int64 snapshot_offset,
                                         int64 snapshot_size) {
  if (g_mapped_natives && g_mapped_snapshot)
    return true;

  base::MemoryMappedFile::Region natives_region =
      base::MemoryMappedFile::Region::kWholeFile;
  if (natives_size != 0 || natives_offset != 0) {
    natives_region =
        base::MemoryMappedFile::Region(natives_offset, natives_size);
  }

  base::MemoryMappedFile::Region snapshot_region =
      base::MemoryMappedFile::Region::kWholeFile;
  if (natives_size != 0 || natives_offset != 0) {
    snapshot_region =
        base::MemoryMappedFile::Region(snapshot_offset, snapshot_size);
  }

  return MapV8Files(base::File(natives_pf), base::File(snapshot_pf),
                    natives_region, snapshot_region);
}

// static
bool V8Initializer::OpenV8FilesForChildProcesses(
    base::PlatformFile* natives_fd_out,
    base::PlatformFile* snapshot_fd_out) {
  base::FilePath natives_data_path;
  base::FilePath snapshot_data_path;
  GetV8FilePaths(&natives_data_path, &snapshot_data_path);

  base::File natives_data_file;
  base::File snapshot_data_file;
  int file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;

  bool success = OpenV8File(natives_data_path, file_flags, natives_data_file) &&
                 OpenV8File(snapshot_data_path, file_flags, snapshot_data_file);
  if (success) {
    *natives_fd_out = natives_data_file.TakePlatformFile();
    *snapshot_fd_out = snapshot_data_file.TakePlatformFile();
  }
  return success;
}

#endif  // V8_USE_EXTERNAL_STARTUP_DATA

// static
void V8Initializer::Initialize(gin::IsolateHolder::ScriptMode mode) {
  static bool v8_is_initialized = false;
  if (v8_is_initialized)
    return;

  v8::V8::InitializePlatform(V8Platform::Get());

  if (gin::IsolateHolder::kStrictMode == mode) {
    static const char use_strict[] = "--use_strict";
    v8::V8::SetFlagsFromString(use_strict, sizeof(use_strict) - 1);
  }

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  v8::StartupData natives;
  natives.data = reinterpret_cast<const char*>(g_mapped_natives->data());
  natives.raw_size = static_cast<int>(g_mapped_natives->length());
  v8::V8::SetNativesDataBlob(&natives);

  v8::StartupData snapshot;
  snapshot.data = reinterpret_cast<const char*>(g_mapped_snapshot->data());
  snapshot.raw_size = static_cast<int>(g_mapped_snapshot->length());
  v8::V8::SetSnapshotDataBlob(&snapshot);
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

  v8::V8::SetEntropySource(&GenerateEntropy);
  v8::V8::Initialize();

  v8_is_initialized = true;
}

// static
void V8Initializer::GetV8ExternalSnapshotData(const char** natives_data_out,
                                              int* natives_size_out,
                                              const char** snapshot_data_out,
                                              int* snapshot_size_out) {
  if (!g_mapped_natives || !g_mapped_snapshot) {
    *natives_data_out = *snapshot_data_out = NULL;
    *natives_size_out = *snapshot_size_out = 0;
    return;
  }
  *natives_data_out = reinterpret_cast<const char*>(g_mapped_natives->data());
  *snapshot_data_out = reinterpret_cast<const char*>(g_mapped_snapshot->data());
  *natives_size_out = static_cast<int>(g_mapped_natives->length());
  *snapshot_size_out = static_cast<int>(g_mapped_snapshot->length());
}

}  // namespace gin

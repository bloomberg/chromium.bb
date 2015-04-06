// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_initializer.h"

#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/sys_string_conversions.h"
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

bool MapV8Files(base::File natives_file,
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
  if (g_mapped_natives && g_mapped_snapshot)
    return true;

  base::FilePath natives_data_path;
  base::FilePath snapshot_data_path;
  GetV8FilePaths(&natives_data_path, &snapshot_data_path);

  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  if (!MapV8Files(base::File(natives_data_path, flags),
                  base::File(snapshot_data_path, flags)))
    return false;

#if defined(V8_VERIFY_EXTERNAL_STARTUP_DATA)
  return VerifyV8SnapshotFile(g_mapped_natives, g_natives_fingerprint) &&
         VerifyV8SnapshotFile(g_mapped_snapshot, g_snapshot_fingerprint);
#else
  return true;
#endif  // V8_VERIFY_EXTERNAL_STARTUP_DATA
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

  int file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::File natives_data_file(natives_data_path, file_flags);
  base::File snapshot_data_file(snapshot_data_path, file_flags);

  if (!natives_data_file.IsValid() || !snapshot_data_file.IsValid())
    return false;

  *natives_fd_out = natives_data_file.TakePlatformFile();
  *snapshot_fd_out = snapshot_data_file.TakePlatformFile();
  return true;
}

#endif  // V8_USE_EXTERNAL_STARTUP_DATA

// static
void V8Initializer::Initialize(gin::IsolateHolder::ScriptMode mode,
                               v8::ArrayBuffer::Allocator* allocator) {
  CHECK(allocator);

  static bool v8_is_initialized = false;
  if (v8_is_initialized)
    return;

  v8::V8::InitializePlatform(V8Platform::Get());
  v8::V8::SetArrayBufferAllocator(allocator);

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

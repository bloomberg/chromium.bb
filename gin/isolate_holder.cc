// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/isolate_holder.h"

#include <stdlib.h>
#include <string.h>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"
#include "crypto/sha2.h"
#include "gin/array_buffer.h"
#include "gin/debug_impl.h"
#include "gin/function_template.h"
#include "gin/per_isolate_data.h"
#include "gin/public/v8_platform.h"
#include "gin/run_microtasks_observer.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif  // OS_MACOSX
#include "base/path_service.h"
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

namespace gin {

namespace {

v8::ArrayBuffer::Allocator* g_array_buffer_allocator = NULL;

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  base::RandBytes(buffer, amount);
  return true;
}

base::MemoryMappedFile* g_mapped_natives = NULL;
base::MemoryMappedFile* g_mapped_snapshot = NULL;

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
bool MapV8Files(base::FilePath* natives_path,
                base::FilePath* snapshot_path,
                int natives_fd = -1,
                int snapshot_fd = -1,
                base::MemoryMappedFile::Region natives_region =
                    base::MemoryMappedFile::Region::kWholeFile,
                base::MemoryMappedFile::Region snapshot_region =
                    base::MemoryMappedFile::Region::kWholeFile) {
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;

  g_mapped_natives = new base::MemoryMappedFile;
  if (!g_mapped_natives->IsValid()) {
#if !defined(OS_WIN)
    if (natives_fd == -1
            ? !g_mapped_natives->Initialize(base::File(*natives_path, flags),
                                            natives_region)
            : !g_mapped_natives->Initialize(base::File(natives_fd),
                                            natives_region)) {
#else
    if (!g_mapped_natives->Initialize(base::File(*natives_path, flags),
                                      natives_region)) {
#endif  // !OS_WIN
      delete g_mapped_natives;
      g_mapped_natives = NULL;
      LOG(FATAL) << "Couldn't mmap v8 natives data file";
      return false;
    }
  }

  g_mapped_snapshot = new base::MemoryMappedFile;
  if (!g_mapped_snapshot->IsValid()) {
#if !defined(OS_WIN)
    if (snapshot_fd == -1
            ? !g_mapped_snapshot->Initialize(base::File(*snapshot_path, flags),
                                             snapshot_region)
            : !g_mapped_snapshot->Initialize(base::File(snapshot_fd),
                                             snapshot_region)) {
#else
    if (!g_mapped_snapshot->Initialize(base::File(*snapshot_path, flags),
                                       snapshot_region)) {
#endif  // !OS_WIN
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

}  // namespace

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)

#if defined(V8_VERIFY_EXTERNAL_STARTUP_DATA)
// Defined in gen/gin/v8_snapshot_fingerprint.cc
extern const unsigned char g_natives_fingerprint[];
extern const unsigned char g_snapshot_fingerprint[];
#endif  // V8_VERIFY_EXTERNAL_STARTUP_DATA

#if !defined(OS_MACOSX)
const int IsolateHolder::kV8SnapshotBasePathKey =
#if defined(OS_ANDROID)
    base::DIR_ANDROID_APP_DATA;
#elif defined(OS_POSIX)
    base::DIR_EXE;
#elif defined(OS_WIN)
    base::DIR_MODULE;
#endif  // OS_ANDROID
#endif  // !OS_MACOSX

const char IsolateHolder::kNativesFileName[] = "natives_blob.bin";
const char IsolateHolder::kSnapshotFileName[] = "snapshot_blob.bin";

// static
bool IsolateHolder::LoadV8Snapshot() {
  if (g_mapped_natives && g_mapped_snapshot)
    return true;

#if !defined(OS_MACOSX)
  base::FilePath data_path;
  PathService::Get(kV8SnapshotBasePathKey, &data_path);
  DCHECK(!data_path.empty());

  base::FilePath natives_path = data_path.AppendASCII(kNativesFileName);
  base::FilePath snapshot_path = data_path.AppendASCII(kSnapshotFileName);
#else  // !defined(OS_MACOSX)
  base::ScopedCFTypeRef<CFStringRef> natives_file_name(
      base::SysUTF8ToCFStringRef(kNativesFileName));
  base::FilePath natives_path = base::mac::PathForFrameworkBundleResource(
      natives_file_name);
  base::ScopedCFTypeRef<CFStringRef> snapshot_file_name(
      base::SysUTF8ToCFStringRef(kSnapshotFileName));
  base::FilePath snapshot_path = base::mac::PathForFrameworkBundleResource(
      snapshot_file_name);
  DCHECK(!natives_path.empty());
  DCHECK(!snapshot_path.empty());
#endif  // !defined(OS_MACOSX)

  if (!MapV8Files(&natives_path, &snapshot_path))
    return false;

#if defined(V8_VERIFY_EXTERNAL_STARTUP_DATA)
  return VerifyV8SnapshotFile(g_mapped_natives, g_natives_fingerprint) &&
         VerifyV8SnapshotFile(g_mapped_snapshot, g_snapshot_fingerprint);
#else
  return true;
#endif  // V8_VERIFY_EXTERNAL_STARTUP_DATA
}

//static
bool IsolateHolder::LoadV8SnapshotFd(int natives_fd,
                               int64 natives_offset,
                               int64 natives_size,
                               int snapshot_fd,
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

  return MapV8Files(
      NULL, NULL, natives_fd, snapshot_fd, natives_region, snapshot_region);
}
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

//static
void IsolateHolder::GetV8ExternalSnapshotData(const char** natives_data_out,
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

IsolateHolder::IsolateHolder() {
  CHECK(g_array_buffer_allocator)
      << "You need to invoke gin::IsolateHolder::Initialize first";
  v8::Isolate::CreateParams params;
  params.entry_hook = DebugImpl::GetFunctionEntryHook();
  params.code_event_handler = DebugImpl::GetJitCodeEventHandler();
  params.constraints.ConfigureDefaults(base::SysInfo::AmountOfPhysicalMemory(),
                                       base::SysInfo::AmountOfVirtualMemory(),
                                       base::SysInfo::NumberOfProcessors());
  isolate_ = v8::Isolate::New(params);
  isolate_data_.reset(new PerIsolateData(isolate_, g_array_buffer_allocator));
#if defined(OS_WIN)
  {
    void* code_range;
    size_t size;
    isolate_->GetCodeRange(&code_range, &size);
    Debug::CodeRangeCreatedCallback callback =
        DebugImpl::GetCodeRangeCreatedCallback();
    if (code_range && size && callback)
      callback(code_range, size);
  }
#endif
}

IsolateHolder::~IsolateHolder() {
  if (task_observer_.get())
    base::MessageLoop::current()->RemoveTaskObserver(task_observer_.get());
#if defined(OS_WIN)
  {
    void* code_range;
    size_t size;
    isolate_->GetCodeRange(&code_range, &size);
    Debug::CodeRangeDeletedCallback callback =
        DebugImpl::GetCodeRangeDeletedCallback();
    if (code_range && callback)
      callback(code_range);
  }
#endif
  isolate_data_.reset();
  isolate_->Dispose();
  isolate_ = NULL;
}

// static
void IsolateHolder::Initialize(ScriptMode mode,
                               v8::ArrayBuffer::Allocator* allocator) {
  CHECK(allocator);
  static bool v8_is_initialized = false;
  if (v8_is_initialized)
    return;
  v8::V8::InitializePlatform(V8Platform::Get());
  v8::V8::SetArrayBufferAllocator(allocator);
  g_array_buffer_allocator = allocator;
  if (mode == gin::IsolateHolder::kStrictMode) {
    static const char use_strict[] = "--use_strict";
    v8::V8::SetFlagsFromString(use_strict, sizeof(use_strict) - 1);
  }
  v8::V8::SetEntropySource(&GenerateEntropy);
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
  v8::V8::Initialize();
  v8_is_initialized = true;
}

void IsolateHolder::AddRunMicrotasksObserver() {
  DCHECK(!task_observer_.get());
  task_observer_.reset(new RunMicrotasksObserver(isolate_));;
  base::MessageLoop::current()->AddTaskObserver(task_observer_.get());
}

void IsolateHolder::RemoveRunMicrotasksObserver() {
  DCHECK(task_observer_.get());
  base::MessageLoop::current()->RemoveTaskObserver(task_observer_.get());
  task_observer_.reset();
}

}  // namespace gin

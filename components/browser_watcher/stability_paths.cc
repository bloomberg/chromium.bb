// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/stability_paths.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

#include <string>

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/browser_watcher/features.h"

#if defined(OS_WIN)

#include "third_party/crashpad/crashpad/util/win/time.h"

namespace browser_watcher {
namespace {

bool GetCreationTime(const base::Process& process, FILETIME* creation_time) {
  FILETIME ignore;
  return ::GetProcessTimes(process.Handle(), creation_time, &ignore, &ignore,
                           &ignore) != 0;
}

}  // namespace

base::FilePath GetStabilityDir(const base::FilePath& user_data_dir) {
  return user_data_dir.AppendASCII("Stability");
}

base::FilePath GetStabilityFileForProcess(base::ProcessId pid,
                                          timeval creation_time,
                                          const base::FilePath& user_data_dir) {
  base::FilePath stability_dir = GetStabilityDir(user_data_dir);

  constexpr uint64_t kMicrosecondsPerSecond = static_cast<uint64_t>(1E6);
  int64_t creation_time_us =
      creation_time.tv_sec * kMicrosecondsPerSecond + creation_time.tv_usec;

  std::string file_name = base::StringPrintf("%u-%lld", pid, creation_time_us);
  return stability_dir.AppendASCII(file_name).AddExtension(
      base::PersistentMemoryAllocator::kFileExtension);
}

bool GetStabilityFileForProcess(const base::Process& process,
                                const base::FilePath& user_data_dir,
                                base::FilePath* file_path) {
  DCHECK(file_path);

  FILETIME creation_time;
  if (!GetCreationTime(process, &creation_time))
    return false;

  // We rely on Crashpad's conversion to ensure the resulting filename is the
  // same as on crash, when the creation time is obtained via Crashpad.
  *file_path = GetStabilityFileForProcess(
      process.Pid(), crashpad::FiletimeToTimevalEpoch(creation_time),
      user_data_dir);
  return true;
}

base::FilePath::StringType GetStabilityFilePattern() {
  return base::FilePath::StringType(FILE_PATH_LITERAL("*-*")) +
         base::PersistentMemoryAllocator::kFileExtension;
}

void MarkStabilityFileForDeletion(const base::FilePath& user_data_dir) {
  if (!base::FeatureList::IsEnabled(
          browser_watcher::kStabilityDebuggingFeature)) {
    return;
  }

  base::FilePath stability_file;
  if (!GetStabilityFileForProcess(base::Process::Current(), user_data_dir,
                                  &stability_file)) {
    // TODO(manzagop): add a metric for this.
    return;
  }

  // Open (with delete) and then immediately close the file by going out of
  // scope. This should cause the stability debugging file to be deleted prior
  // to the next execution.
  base::File file(stability_file, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_DELETE_ON_CLOSE);
}

}  // namespace browser_watcher

#endif  // defined(OS_WIN)

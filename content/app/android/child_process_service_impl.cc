// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/child_process_service_impl.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/command_line.h"
#include "base/file_descriptor_store.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/global_descriptors.h"
#include "jni/ChildProcessServiceImpl_jni.h"
#include "services/service_manager/embedder/shared_file_util.h"
#include "services/service_manager/embedder/switches.h"

using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;

namespace content {

void RegisterFileDescriptors(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jintArray>& j_ids,
                             const JavaParamRef<jintArray>& j_fds,
                             const JavaParamRef<jlongArray>& j_offsets,
                             const JavaParamRef<jlongArray>& j_sizes) {
  std::vector<int> ids;
  base::android::JavaIntArrayToIntVector(env, j_ids, &ids);
  std::vector<int> fds;
  base::android::JavaIntArrayToIntVector(env, j_fds, &fds);
  std::vector<int64_t> offsets;
  base::android::JavaLongArrayToInt64Vector(env, j_offsets, &offsets);
  std::vector<int64_t> sizes;
  base::android::JavaLongArrayToInt64Vector(env, j_sizes, &sizes);

  DCHECK_EQ(ids.size(), fds.size());
  DCHECK_EQ(fds.size(), offsets.size());
  DCHECK_EQ(offsets.size(), sizes.size());

  std::map<int, std::string> ids_to_keys;
  std::string file_switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          service_manager::switches::kSharedFiles);
  if (!file_switch_value.empty()) {
    base::Optional<std::map<int, std::string>> ids_to_keys_from_command_line =
        service_manager::ParseSharedFileSwitchValue(file_switch_value);
    if (ids_to_keys_from_command_line) {
      ids_to_keys = std::move(*ids_to_keys_from_command_line);
    }
  }

  for (size_t i = 0; i < ids.size(); i++) {
    base::MemoryMappedFile::Region region = {offsets.at(i), sizes.at(i)};
    int id = ids.at(i);
    int fd = fds.at(i);
    auto iter = ids_to_keys.find(id);
    if (iter != ids_to_keys.end()) {
      base::FileDescriptorStore::GetInstance().Set(iter->second,
                                                   base::ScopedFD(fd), region);
    } else {
      base::GlobalDescriptors::GetInstance()->Set(id, fd, region);
    }
  }
}

void ExitChildProcess(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  VLOG(0) << "ChildProcessServiceImpl: Exiting child process.";
  base::android::LibraryLoaderExitHook();
  _exit(0);
}

bool RegisterChildProcessServiceImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content

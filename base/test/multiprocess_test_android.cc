// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/posix/global_descriptors.h"
#include "base/test/multiprocess_test.h"

#include <unistd.h>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "testing/multiprocess_func_list.h"

namespace base {

// A very basic implementation for Android. On Android tests can run in an APK
// and we don't have an executable to exec*. This implementation does the bare
// minimum to execute the method specified by procname (in the child process).
//  - |base_command_line| is ignored.
//  - All options except |fds_to_remap| are ignored.
ProcessHandle SpawnMultiProcessTestChild(const std::string& procname,
                                         const CommandLine& base_command_line,
                                         const LaunchOptions& options) {
  // TODO(viettrungluu): The FD-remapping done below is wrong in the presence of
  // cycles (e.g., fd1 -> fd2, fd2 -> fd1). crbug.com/326576
  FileHandleMappingVector empty;
  const FileHandleMappingVector* fds_to_remap =
      options.fds_to_remap ? options.fds_to_remap : &empty;

  pid_t pid = fork();

  if (pid < 0) {
    PLOG(ERROR) << "fork";
    return kNullProcessHandle;
  }
  if (pid > 0) {
    // Parent process.
    return pid;
  }
  // Child process.
  std::hash_set<int> fds_to_keep_open;
  for (FileHandleMappingVector::const_iterator it = fds_to_remap->begin();
       it != fds_to_remap->end(); ++it) {
    fds_to_keep_open.insert(it->first);
  }
  // Keep standard FDs (stdin, stdout, stderr, etc.) open since this
  // is not meant to spawn a daemon.
  int base = GlobalDescriptors::kBaseDescriptor;
  for (int fd = base; fd < getdtablesize(); ++fd) {
    if (fds_to_keep_open.find(fd) == fds_to_keep_open.end()) {
      close(fd);
    }
  }
  for (FileHandleMappingVector::const_iterator it = fds_to_remap->begin();
       it != fds_to_remap->end(); ++it) {
    int old_fd = it->first;
    int new_fd = it->second;
    if (dup2(old_fd, new_fd) < 0) {
      PLOG(FATAL) << "dup2";
    }
    close(old_fd);
  }
  _exit(multi_process_function_list::InvokeChildProcessTest(procname));
  return 0;
}

}  // namespace base

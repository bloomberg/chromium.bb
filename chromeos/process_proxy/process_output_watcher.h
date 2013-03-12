// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PROCESS_PROXY_PROCESS_OUTPUT_WATCHER_H_
#define CHROMEOS_PROCESS_PROXY_PROCESS_OUTPUT_WATCHER_H_

#include <string>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"

namespace {

const int kReadBufferSize = 256;

}  // namespace

namespace chromeos {

enum ProcessOutputType {
  PROCESS_OUTPUT_TYPE_OUT,
  PROCESS_OUTPUT_TYPE_ERR,
  PROCESS_OUTPUT_TYPE_EXIT
};

typedef base::Callback<void(ProcessOutputType, const std::string&)>
      ProcessOutputCallback;

// This class should live on its own thread because running class makes
// underlying thread block. It deletes itself when watching is stopped.
class CHROMEOS_EXPORT ProcessOutputWatcher {
 public:
  ProcessOutputWatcher(int out_fd, int stop_fd,
                       const ProcessOutputCallback& callback);

  // This will block current thread!!!!
  void Start();

 private:
  // The object will destroy itself when it stops watching process output.
  ~ProcessOutputWatcher();

  // Listens to output from supplied fds. It guarantees data written to one fd
  // will be reported in order that it has been written (this is not true across
  // fds, it would be nicer if it was).
  void WatchProcessOutput();

  // Verifies that fds that we got are properly set.
  void VerifyFileDescriptor(int fd);

  // Reads data from fd, and when it's done, invokes callback function.
  void ReadFromFd(ProcessOutputType type, int* fd);

  // It will just delete this.
  void OnStop();

  char read_buffer_[kReadBufferSize];
  ssize_t read_buffer_size_;

  int out_fd_;
  int stop_fd_;
  int max_fd_;

  // Callback that will be invoked when some output is detected.
  ProcessOutputCallback on_read_callback_;

  DISALLOW_COPY_AND_ASSIGN(ProcessOutputWatcher);
};

}  // namespace chromeos

#endif  // CHROMEOS_PROCESS_PROXY_PROCESS_OUTPUT_WATCHER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CHILD_PROCESS_CRASH_OBSERVER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CHILD_PROCESS_CRASH_OBSERVER_ANDROID_H_

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "components/crash/content/browser/crash_dump_observer_android.h"

namespace breakpad {

class ChildProcessCrashObserver : public breakpad::CrashDumpObserver::Client {
 public:
  // |increase_crash_cb is| the callback to run after processing minidump file.
  // For now this callback is used to increase render crash counter based on
  // processing minidump result.
  ChildProcessCrashObserver(const base::FilePath crash_dump_dir,
                            int descriptor_id,
                            const base::Closure& increase_crash_cb);
  ~ChildProcessCrashObserver() override;

  // breakpad::CrashDumpObserver::Client implementation:
  void OnChildStart(int child_process_id,
                    content::PosixFileDescriptorInfo* mappings) override;
  void OnChildExit(int child_process_id,
                   base::ProcessHandle pid,
                   content::ProcessType process_type,
                   base::TerminationStatus termination_status,
                   base::android::ApplicationState app_state) override;

 private:
  base::FilePath crash_dump_dir_;
  // The id used to identify the file descriptor in the set of file
  // descriptor mappings passed to the child process.
  int descriptor_id_;

  base::Callback<void(bool)> increase_crash_cb_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessCrashObserver);
};

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CHILD_PROCESS_CRASH_OBSERVER_ANDROID_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/crash_util.h"

#include <stdlib.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/version.h"
#include "chromecast/crash/app_state_tracker.h"
#include "chromecast/crash/linux/dummy_minidump_generator.h"
#include "chromecast/crash/linux/minidump_writer.h"

namespace chromecast {

namespace {

// This can be set to a callback for testing. This allows us to inject a fake
// dumpstate routine to avoid calling an executable during an automated test.
// This value should not be mutated through any other function except
// CrashUtil::SetDumpStateCbForTest().
static base::Callback<int(const std::string&)>* g_dumpstate_cb = nullptr;

}  // namespace

// static
uint64_t CrashUtil::GetCurrentTimeMs() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();
}

// static
bool CrashUtil::RequestUploadCrashDump(
    const std::string& existing_minidump_path,
    const std::string& crashed_process_name,
    uint64_t crashed_process_start_time_ms) {
  LOG(INFO) << "Request to upload crash dump " << existing_minidump_path
            << " for process " << crashed_process_name;

  // Note: Do not use Chromium IO methods in this function. When cast_shell
  // crashes, this function can be called by any thread, which may not allow IO.
  // Use stdlib system calls for IO instead.
  uint64_t uptime_ms = GetCurrentTimeMs() - crashed_process_start_time_ms;
  MinidumpParams params(crashed_process_name,
                        uptime_ms,
                        "",  // suffix
                        AppStateTracker::GetPreviousApp(),
                        AppStateTracker::GetCurrentApp(),
                        AppStateTracker::GetLastLaunchedApp(),
                        CAST_BUILD_RELEASE,
                        CAST_BUILD_INCREMENTAL);
  DummyMinidumpGenerator minidump_generator(existing_minidump_path);

  base::FilePath filename = base::FilePath(existing_minidump_path).BaseName();

  scoped_ptr<MinidumpWriter> writer;
  if (g_dumpstate_cb) {
    writer.reset(new MinidumpWriter(
        &minidump_generator, filename.value(), params, *g_dumpstate_cb));
  } else {
    writer.reset(
        new MinidumpWriter(&minidump_generator, filename.value(), params));
  }
  bool success = false;
  writer->set_non_blocking(false);
  success = (0 == writer->Write());  // error already logged.

  // In case the file is still in $TEMP, remove it.
  if (remove(existing_minidump_path.c_str()) < 0 && errno != ENOENT) {
    LOG(ERROR) << "Unable to delete temp minidump file "
               << existing_minidump_path << ": " << strerror(errno);
    success = false;
  }

  // Use std::endl to flush the log stream in case this process exits.
  LOG(INFO) << "Request to upload crash dump finished. "
            << "Exit now if it is main process that crashed." << std::endl;

  return success;
}

void CrashUtil::SetDumpStateCbForTest(
    const base::Callback<int(const std::string&)>& cb) {
  DCHECK(!g_dumpstate_cb);
  g_dumpstate_cb = new base::Callback<int(const std::string&)>(cb);
}

}  // namespace chromecast

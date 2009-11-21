// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_info_snapshot.h"

#include <iostream>
#include <sstream>

#include "base/string_util.h"
#include "base/thread.h"

// Implementation for the Mac; calls '/bin/ps' for information when
// |Sample()| is called.

// Default constructor.
ProcessInfoSnapshot::ProcessInfoSnapshot() { }

// Destructor: just call |Reset()| to release everything.
ProcessInfoSnapshot::~ProcessInfoSnapshot() {
  Reset();
}

// Capture the information by calling '/bin/ps'.
// Note: we ignore the "tsiz" (text size) display option of ps because it's
// always zero (tested on 10.5 and 10.6).
bool ProcessInfoSnapshot::Sample(std::vector<base::ProcessId> pid_list) {
  const char* kPsPathName = "/bin/ps";
  Reset();

  std::vector<std::string> argv;
  argv.push_back(kPsPathName);
  // Get PID, PPID, (real) UID, effective UID, resident set size, virtual memory
  // size, and command.
  argv.push_back("-o");
  argv.push_back("pid=,ppid=,ruid=,uid=,rss=,vsz=,comm=");
  // Only display the specified PIDs.
  for (std::vector<base::ProcessId>::iterator it = pid_list.begin();
      it != pid_list.end(); ++it) {
    argv.push_back("-p");
    argv.push_back(Int64ToString(static_cast<int64>(*it)));
  }

  std::string output;
  CommandLine command_line(argv);
  if (!base::GetAppOutputRestricted(command_line,
                                    &output, (pid_list.size() + 10) * 100)) {
    LOG(ERROR) << "Failure running " << kPsPathName << " to acquire data.";
    return false;
  }

  std::istringstream in(output, std::istringstream::in);
  std::string line;

  // Process lines until done.
  while (true) {
    ProcInfoEntry proc_info;

    // The format is as specified above to ps (see ps(1)):
    //   "-o pid=,ppid=,ruid=,uid=,rss=,vsz=,comm=".
    // Try to read the PID; if we get it, we should be able to get the rest of
    // the line.
    in >> proc_info.pid;
    if (in.eof())
      break;
    in >> proc_info.ppid;
    in >> proc_info.uid;
    in >> proc_info.euid;
    in >> proc_info.rss;
    in >> proc_info.vsize;
    in.ignore(1, ' ');                    // Eat the space.
    std::getline(in, proc_info.command);  // Get the rest of the line.
    if (!in.good()) {
      LOG(ERROR) << "Error parsing output from " << kPsPathName << ".";
      return false;
    }

    // Make sure the new PID isn't already in our list.
    if (proc_info_entries_.find(proc_info.pid) != proc_info_entries_.end()) {
      LOG(ERROR) << "Duplicate PID in output from " << kPsPathName << ".";
      return false;
    }

    if (!proc_info.pid || ! proc_info.vsize) {
      LOG(WARNING) << "Invalid data from " << kPsPathName << ".";
      return false;
    }

    // Record the process information.
    proc_info_entries_[proc_info.pid] = proc_info;
  }

  return true;
}

// Clear all the stored information.
void ProcessInfoSnapshot::Reset() {
  proc_info_entries_.clear();
}

bool ProcessInfoSnapshot::GetProcInfo(int pid,
                                      ProcInfoEntry* proc_info) const {
  std::map<int,ProcInfoEntry>::const_iterator it = proc_info_entries_.find(pid);
  if (it == proc_info_entries_.end())
    return false;

  *proc_info = it->second;
  return true;
}

bool ProcessInfoSnapshot::GetCommittedKBytesOfPID(
    int pid,
    base::CommittedKBytes* usage) const {
  // Try to avoid crashing on a bug; stats aren't usually so crucial.
  if (!usage) {
    NOTREACHED();
    return false;
  }

  // Failure of |GetProcInfo()| is "normal", due to racing.
  ProcInfoEntry proc_info;
  if (!GetProcInfo(pid, &proc_info)) {
    usage->priv = 0;
    usage->mapped = 0;
    usage->image = 0;
    return false;
  }

  usage->priv = proc_info.vsize;
  usage->mapped = 0;
  usage->image = 0;
  return true;
}

bool ProcessInfoSnapshot::GetWorkingSetKBytesOfPID(
    int pid,
    base::WorkingSetKBytes* ws_usage) const {
  // Try to avoid crashing on a bug; stats aren't usually so crucial.
  if (!ws_usage) {
    NOTREACHED();
    return false;
  }

  // Failure of |GetProcInfo()| is "normal", due to racing.
  ProcInfoEntry proc_info;
  if (!GetProcInfo(pid, &proc_info)) {
    ws_usage->priv = 0;
    ws_usage->shareable = 0;
    ws_usage->shared = 0;
    return false;
  }

  ws_usage->priv = 0;
  ws_usage->shareable = proc_info.rss;
  ws_usage->shared = 0;
  return true;
}

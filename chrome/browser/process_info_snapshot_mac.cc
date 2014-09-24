// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_info_snapshot.h"

#include <sys/sysctl.h>

#include <sstream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"

// Default constructor.
ProcessInfoSnapshot::ProcessInfoSnapshot() { }

// Destructor: just call |Reset()| to release everything.
ProcessInfoSnapshot::~ProcessInfoSnapshot() {
  Reset();
}

const size_t ProcessInfoSnapshot::kMaxPidListSize = 1000;

static bool GetKInfoForProcessID(pid_t pid, kinfo_proc* kinfo) {
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
  size_t len = sizeof(*kinfo);
  if (sysctl(mib, arraysize(mib), kinfo, &len, NULL, 0) != 0) {
    PLOG(ERROR) << "sysctl() for KERN_PROC";
    return false;
  }

  if (len == 0) {
    // If the process isn't found then sysctl returns a length of 0.
    return false;
  }

  return true;
}

static bool GetExecutableNameForProcessID(
    pid_t pid,
    std::string* executable_name) {
  if (!executable_name) {
    NOTREACHED();
    return false;
  }

  static int s_arg_max = 0;
  if (s_arg_max == 0) {
    int mib[] = {CTL_KERN, KERN_ARGMAX};
    size_t size = sizeof(s_arg_max);
    if (sysctl(mib, arraysize(mib), &s_arg_max, &size, NULL, 0) != 0)
      PLOG(ERROR) << "sysctl() for KERN_ARGMAX";
  }

  if (s_arg_max == 0)
    return false;

  int mib[] = {CTL_KERN, KERN_PROCARGS, pid};
  size_t size = s_arg_max;
  executable_name->resize(s_arg_max + 1);
  if (sysctl(mib, arraysize(mib), &(*executable_name)[0],
             &size, NULL, 0) != 0) {
    // Don't log the error since it's normal for this to fail.
    return false;
  }

  // KERN_PROCARGS returns multiple NULL terminated strings. Truncate
  // executable_name to just the first string.
  size_t end_pos = executable_name->find('\0');
  if (end_pos == std::string::npos) {
    return false;
  }

  executable_name->resize(end_pos);
  return true;
}

// Converts a byte unit such as 'K' or 'M' into the scale for the unit.
// The scale can then be used to calculate the number of bytes in a value.
// The units are based on humanize_number(). See:
// http://www.opensource.apple.com/source/libutil/libutil-21/humanize_number.c
static bool ConvertByteUnitToScale(char unit, uint64_t* out_scale) {
  int shift = 0;
  switch (unit) {
    case 'B':
      shift = 0;
      break;
    case 'K':
    case 'k':
      shift = 1;
      break;
    case 'M':
      shift = 2;
      break;
    case 'G':
      shift = 3;
      break;
    case 'T':
      shift = 4;
      break;
    case 'P':
      shift = 5;
      break;
    case 'E':
      shift = 6;
      break;
    default:
      return false;
  }

  uint64_t scale = 1;
  for (int i = 0; i < shift; i++)
    scale *= 1024;
  *out_scale = scale;

  return true;
}

// Capture the information by calling '/bin/ps'.
// Note: we ignore the "tsiz" (text size) display option of ps because it's
// always zero (tested on 10.5 and 10.6).
static bool GetProcessMemoryInfoUsingPS(
    const std::vector<base::ProcessId>& pid_list,
    std::map<int,ProcessInfoSnapshot::ProcInfoEntry>& proc_info_entries) {
  const base::FilePath kProgram("/bin/ps");
  CommandLine command_line(kProgram);

  // Get resident set size, virtual memory size.
  command_line.AppendArg("-o");
  command_line.AppendArg("pid=,rss=,vsz=");
  // Only display the specified PIDs.
  for (std::vector<base::ProcessId>::const_iterator it = pid_list.begin();
       it != pid_list.end(); ++it) {
    command_line.AppendArg("-p");
    command_line.AppendArg(base::Int64ToString(static_cast<int64>(*it)));
  }

  std::string output;
  // Limit output read to a megabyte for safety.
  if (!base::GetAppOutputRestricted(command_line, &output, 1024 * 1024)) {
    LOG(ERROR) << "Failure running " << kProgram.value() << " to acquire data.";
    return false;
  }

  std::istringstream in(output, std::istringstream::in);

  // Process lines until done.
  while (true) {
    // The format is as specified above to ps (see ps(1)):
    //   "-o pid=,rss=,vsz=".
    // Try to read the PID; if we get it, we should be able to get the rest of
    // the line.
    pid_t pid;
    in >> pid;
    if (in.eof())
      break;

    ProcessInfoSnapshot::ProcInfoEntry proc_info = proc_info_entries[pid];
    proc_info.pid = pid;
    in >> proc_info.rss;
    in >> proc_info.vsize;
    proc_info.rss *= 1024;                // Convert from kilobytes to bytes.
    proc_info.vsize *= 1024;

    // If the fail or bad bits were set, then there was an error reading input.
    if (in.fail()) {
      LOG(ERROR) << "Error parsing output from " << kProgram.value() << ".";
      return false;
    }

    if (!proc_info.pid || ! proc_info.vsize) {
      LOG(WARNING) << "Invalid data from " << kProgram.value() << ".";
      return false;
    }

    // Record the process information.
    proc_info_entries[proc_info.pid] = proc_info;

    // Ignore the rest of the line.
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }

  return true;
}

static bool GetProcessMemoryInfoUsingTop(
    std::map<int,ProcessInfoSnapshot::ProcInfoEntry>& proc_info_entries) {
  const base::FilePath kProgram("/usr/bin/top");
  CommandLine command_line(kProgram);

  // -stats tells top to print just the given fields as ordered.
  command_line.AppendArg("-stats");
  command_line.AppendArg("pid,"    // Process ID
                         "rsize,"  // Resident memory
                         "rshrd,"  // Resident shared memory
                         "rprvt,"  // Resident private memory
                         "vsize"); // Total virtual memory
  // Run top in logging (non-interactive) mode.
  command_line.AppendArg("-l");
  command_line.AppendArg("1");
  // Set the delay between updates to 0.
  command_line.AppendArg("-s");
  command_line.AppendArg("0");

  std::string output;
  // Limit output read to a megabyte for safety.
  if (!base::GetAppOutputRestricted(command_line, &output, 1024 * 1024)) {
    LOG(ERROR) << "Failure running " << kProgram.value() << " to acquire data.";
    return false;
  }

  // Process lines until done. Lines should look something like this:
  // PID    RSIZE  RSHRD  RPRVT  VSIZE
  // 58539  1276K+ 336K+  740K+  2378M+
  // 58485  1888K+ 592K+  1332K+ 2383M+
  std::istringstream top_in(output, std::istringstream::in);
  std::string line;
  while (std::getline(top_in, line)) {
    std::istringstream in(line, std::istringstream::in);

    // Try to read the PID.
    pid_t pid;
    in >> pid;
    if (in.fail())
      continue;

    // Make sure that caller is interested in this process.
    if (proc_info_entries.find(pid) == proc_info_entries.end())
      continue;

    // Skip the - or + sign that top puts after the pid.
    in.get();

    uint64_t values[4];
    size_t i;
    for (i = 0; i < arraysize(values); i++) {
      in >> values[i];
      if (in.fail())
        break;
      std::string unit;
      in >> unit;
      if (in.fail())
        break;

      if (unit.empty())
        break;

      uint64_t scale;
      if (!ConvertByteUnitToScale(unit[0], &scale))
        break;
      values[i] *= scale;
    }
    if (i != arraysize(values))
      continue;

    ProcessInfoSnapshot::ProcInfoEntry proc_info = proc_info_entries[pid];
    proc_info.rss = values[0];
    proc_info.rshrd = values[1];
    proc_info.rprvt = values[2];
    proc_info.vsize = values[3];
    // Record the process information.
    proc_info_entries[proc_info.pid] = proc_info;
  }

  return true;
}

bool ProcessInfoSnapshot::Sample(std::vector<base::ProcessId> pid_list) {
  Reset();

  // Nothing to do if no PIDs given.
  if (pid_list.empty())
    return true;
  if (pid_list.size() > kMaxPidListSize) {
    // The spec says |pid_list| *must* not have more than this many entries.
    NOTREACHED();
    return false;
  }

  // Get basic process info from KERN_PROC.
  for (std::vector<base::ProcessId>::iterator it = pid_list.begin();
       it != pid_list.end(); ++it) {
    ProcInfoEntry proc_info;
    proc_info.pid = *it;

    kinfo_proc kinfo;
    if (!GetKInfoForProcessID(*it, &kinfo))
      return false;

    proc_info.ppid = kinfo.kp_eproc.e_ppid;
    proc_info.uid = kinfo.kp_eproc.e_pcred.p_ruid;
    proc_info.euid = kinfo.kp_eproc.e_ucred.cr_uid;
    // Note, p_comm is truncated to 16 characters.
    proc_info.command = kinfo.kp_proc.p_comm;
    proc_info_entries_[*it] = proc_info;
  }

  // Use KERN_PROCARGS to get the full executable name. This may fail if this
  // process doesn't have privileges to inspect the target process.
  for (std::vector<base::ProcessId>::iterator it = pid_list.begin();
       it != pid_list.end(); ++it) {
    std::string exectuable_name;
    if (GetExecutableNameForProcessID(*it, &exectuable_name)) {
      ProcInfoEntry proc_info = proc_info_entries_[*it];
      proc_info.command = exectuable_name;
    }
  }

  // In OSX 10.9+, top no longer returns any useful information. 'rshrd' is no
  // longer supported, and 'rprvt' and 'vsize' return N/A. 'rsize' still works,
  // but the information is also available from ps.
  // http://crbug.com/383553
  if (base::mac::IsOSMavericksOrLater())
    return GetProcessMemoryInfoUsingPS(pid_list, proc_info_entries_);

  // Get memory information using top.
  bool memory_info_success = GetProcessMemoryInfoUsingTop(proc_info_entries_);

  // If top didn't work then fall back to ps.
  if (!memory_info_success) {
    memory_info_success = GetProcessMemoryInfoUsingPS(pid_list,
                                                      proc_info_entries_);
  }

  return memory_info_success;
}

// Clear all the stored information.
void ProcessInfoSnapshot::Reset() {
  proc_info_entries_.clear();
}

ProcessInfoSnapshot::ProcInfoEntry::ProcInfoEntry()
    : pid(0),
      ppid(0),
      uid(0),
      euid(0),
      rss(0),
      rshrd(0),
      rprvt(0),
      vsize(0) {
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

  usage->priv = proc_info.vsize / 1024;
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

  ws_usage->priv = proc_info.rprvt / 1024;
  ws_usage->shareable = proc_info.rss / 1024;
  ws_usage->shared = proc_info.rshrd / 1024;
  return true;
}

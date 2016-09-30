// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/environment_data_collection.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

// Populates |process| with platform-specific data related to the chrome browser
// process.
void CollectPlatformProcessData(
    ClientIncidentReport_EnvironmentData_Process* process);

// Populates |os_data| with platform-specific data related to the OS.
void CollectPlatformOSData(ClientIncidentReport_EnvironmentData_OS* os_data);

namespace {

ClientIncidentReport_EnvironmentData_Process_Channel MapChannelToProtobuf(
    version_info::Channel channel) {
  typedef ClientIncidentReport_EnvironmentData_Process Process;
  switch (channel) {
    case version_info::Channel::CANARY:
      return Process::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return Process::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return Process::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return Process::CHANNEL_STABLE;
    default:
      return Process::CHANNEL_UNKNOWN;
  }
}

// Populates |process| with data related to the chrome browser process.
void CollectProcessData(ClientIncidentReport_EnvironmentData_Process* process) {
  // TODO(grt): Move this logic into VersionInfo (it also appears in
  // ChromeMetricsServiceClient).
  std::string version(version_info::GetVersionNumber());
#if defined(ARCH_CPU_64_BITS)
  version += "-64";
#endif  // defined(ARCH_CPU_64_BITS)
  if (!version_info::IsOfficialBuild())
    version += "-devel";
  process->set_version(version);

  process->set_chrome_update_channel(
      MapChannelToProtobuf(chrome::GetChannel()));

  CollectPlatformProcessData(process);
}

}  // namespace

void CollectEnvironmentData(ClientIncidentReport_EnvironmentData* data) {
  // Toggling priority only makes sense in a thread pool.
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  // Reset priority when done with this task.
  // TODO(fdoray): Post this task to the TaskScheduler with a BACKGROUND
  // priority instead of toggling the priority within the task.
  base::ScopedClosureRunner restore_priority(
      base::Bind(&base::PlatformThread::SetCurrentThreadPriority,
                 base::PlatformThread::GetCurrentThreadPriority()));
  // Lower priority for this task.
  base::PlatformThread::SetCurrentThreadPriority(
      base::ThreadPriority::BACKGROUND);

  // OS
  {
    ClientIncidentReport_EnvironmentData_OS* os = data->mutable_os();
    os->set_os_name(base::SysInfo::OperatingSystemName());
    os->set_os_version(base::SysInfo::OperatingSystemVersion());
    CollectPlatformOSData(os);
  }

  // Machine
  {
    base::CPU cpu_info;
    ClientIncidentReport_EnvironmentData_Machine* machine =
        data->mutable_machine();
    machine->set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
    machine->set_cpu_vendor(cpu_info.vendor_name());
    machine->set_cpuid(cpu_info.signature());
  }

  // Process
  CollectProcessData(data->mutable_process());
}

#if !defined(OS_WIN)
void CollectPlatformProcessData(
    ClientIncidentReport_EnvironmentData_Process* process) {
  // Empty implementation for platforms that do not (yet) have their own
  // implementations.
}

void CollectPlatformOSData(ClientIncidentReport_EnvironmentData_OS* os_data) {
  // Empty implementation for platforms that do not (yet) have their own
  // implementations.
}
#endif

}  // namespace safe_browsing

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <ntddscsi.h>
#include <windows.h>
#include <winioctl.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/windows_version.h"

namespace {

// Semi-copy of similarly named struct from ata.h in WinDDK.
struct IDENTIFY_DEVICE_DATA {
  USHORT UnusedWords[217];
  USHORT NominalMediaRotationRate;
  USHORT MoreUnusedWords[38];
};
COMPILE_ASSERT(sizeof(IDENTIFY_DEVICE_DATA) == 512, IdentifyDeviceDataSize);

struct AtaRequest {
  ATA_PASS_THROUGH_EX query;
  IDENTIFY_DEVICE_DATA result;
};

int64 AmountOfMemory(DWORDLONG MEMORYSTATUSEX::* memory_field) {
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(memory_info);
  if (!GlobalMemoryStatusEx(&memory_info)) {
    NOTREACHED();
    return 0;
  }

  int64 rv = static_cast<int64>(memory_info.*memory_field);
  return rv < 0 ? kint64max : rv;
}

}  // namespace

namespace base {

// static
int SysInfo::NumberOfProcessors() {
  return win::OSInfo::GetInstance()->processors();
}

// static
int64 SysInfo::AmountOfPhysicalMemory() {
  return AmountOfMemory(&MEMORYSTATUSEX::ullTotalPhys);
}

// static
int64 SysInfo::AmountOfAvailablePhysicalMemory() {
  return AmountOfMemory(&MEMORYSTATUSEX::ullAvailPhys);
}

// static
int64 SysInfo::AmountOfVirtualMemory() {
  return 0;
}

// static
int64 SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  ThreadRestrictions::AssertIOAllowed();

  ULARGE_INTEGER available, total, free;
  if (!GetDiskFreeSpaceExW(path.value().c_str(), &available, &total, &free))
    return -1;

  int64 rv = static_cast<int64>(available.QuadPart);
  return rv < 0 ? kint64max : rv;
}

bool SysInfo::HasSeekPenalty(const FilePath& path, bool* has_seek_penalty) {
  ThreadRestrictions::AssertIOAllowed();

  DCHECK(path.IsAbsolute());
  DCHECK(has_seek_penalty);

  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  int flags = File::FLAG_OPEN;
  const bool win7_or_higher = win::GetVersion() >= win::VERSION_WIN7;
  if (!win7_or_higher)
    flags |= File::FLAG_READ | File::FLAG_WRITE;

  File volume(FilePath(L"\\\\.\\" + components[0]), flags);
  if (!volume.IsValid())
    return false;

  if (win7_or_higher) {
    STORAGE_PROPERTY_QUERY query = {};
    query.QueryType = PropertyStandardQuery;
    query.PropertyId = StorageDeviceSeekPenaltyProperty;

    DEVICE_SEEK_PENALTY_DESCRIPTOR result;
    DWORD bytes_returned;
    BOOL success = DeviceIoControl(volume.GetPlatformFile(),
                                   IOCTL_STORAGE_QUERY_PROPERTY,
                                   &query, sizeof(query),
                                   &result, sizeof(result),
                                   &bytes_returned, NULL);
    if (success == FALSE || bytes_returned < sizeof(result))
      return false;

    *has_seek_penalty = result.IncursSeekPenalty != FALSE;
  } else {
    AtaRequest request = {};
    request.query.AtaFlags = ATA_FLAGS_DATA_IN;
    request.query.CurrentTaskFile[6] = ID_CMD;
    request.query.DataBufferOffset = sizeof(request.query);
    request.query.DataTransferLength = sizeof(request.result);
    request.query.Length = sizeof(request.query);
    request.query.TimeOutValue = 10;

    DWORD bytes_returned;
    BOOL success = DeviceIoControl(volume.GetPlatformFile(),
                                   IOCTL_ATA_PASS_THROUGH,
                                   &request, sizeof(request),
                                   &request, sizeof(request),
                                   &bytes_returned, NULL);
    if (success == FALSE || bytes_returned < sizeof(request) ||
        request.query.CurrentTaskFile[0]) {
      return false;
    }

    *has_seek_penalty = request.result.NominalMediaRotationRate != 1;
  }

  return true;
}

// static
std::string SysInfo::OperatingSystemName() {
  return "Windows NT";
}

// static
std::string SysInfo::OperatingSystemVersion() {
  win::OSInfo* os_info = win::OSInfo::GetInstance();
  win::OSInfo::VersionNumber version_number = os_info->version_number();
  std::string version(StringPrintf("%d.%d", version_number.major,
                                   version_number.minor));
  win::OSInfo::ServicePack service_pack = os_info->service_pack();
  if (service_pack.major != 0) {
    version += StringPrintf(" SP%d", service_pack.major);
    if (service_pack.minor != 0)
      version += StringPrintf(".%d", service_pack.minor);
  }
  return version;
}

// TODO: Implement OperatingSystemVersionComplete, which would include
// patchlevel/service pack number.
// See chrome/browser/feedback/feedback_util.h, FeedbackUtil::SetOSVersion.

// static
std::string SysInfo::OperatingSystemArchitecture() {
  win::OSInfo::WindowsArchitecture arch =
      win::OSInfo::GetInstance()->architecture();
  switch (arch) {
    case win::OSInfo::X86_ARCHITECTURE:
      return "x86";
    case win::OSInfo::X64_ARCHITECTURE:
      return "x86_64";
    case win::OSInfo::IA64_ARCHITECTURE:
      return "ia64";
    default:
      return "";
  }
}

// static
std::string SysInfo::CPUModelName() {
  return win::OSInfo::GetInstance()->processor_model_name();
}

// static
size_t SysInfo::VMAllocationGranularity() {
  return win::OSInfo::GetInstance()->allocation_granularity();
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32* major_version,
                                            int32* minor_version,
                                            int32* bugfix_version) {
  win::OSInfo* os_info = win::OSInfo::GetInstance();
  *major_version = os_info->version_number().major;
  *minor_version = os_info->version_number().minor;
  *bugfix_version = 0;
}

}  // namespace base

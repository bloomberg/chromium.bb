// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include <windows.h>
#include <ntddscsi.h>
#include <winioctl.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"

namespace metrics {

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

struct PaddedAtaRequest {
  DWORD bytes_returned;  // Intentionally above |request| for alignment.
  AtaRequest request;
 private:
  // Prevents some crashes from bad drivers. http://crbug.com/514822
  BYTE kUnusedBadDriverSpace[256] ALLOW_UNUSED_TYPE;
};

}  // namespace

// static
bool DriveMetricsProvider::HasSeekPenalty(const base::FilePath& path,
                                          bool* has_seek_penalty) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);

  int flags = base::File::FLAG_OPEN;
  bool win7_or_higher = base::win::GetVersion() >= base::win::VERSION_WIN7;
  if (!win7_or_higher)
    flags |= base::File::FLAG_READ | base::File::FLAG_WRITE;

  base::File volume(base::FilePath(L"\\\\.\\" + components[0]), flags);
  if (!volume.IsValid())
    return false;

  if (win7_or_higher) {
    STORAGE_PROPERTY_QUERY query = {};
    query.QueryType = PropertyStandardQuery;
    query.PropertyId = StorageDeviceSeekPenaltyProperty;

    DEVICE_SEEK_PENALTY_DESCRIPTOR result;
    DWORD bytes_returned;

    BOOL success = DeviceIoControl(
        volume.GetPlatformFile(), IOCTL_STORAGE_QUERY_PROPERTY, &query,
        sizeof(query), &result, sizeof(result), &bytes_returned, NULL);

    if (success == FALSE || bytes_returned < sizeof(result))
      return false;

    *has_seek_penalty = result.IncursSeekPenalty != FALSE;
  } else {
    PaddedAtaRequest padded = {};

    AtaRequest* request = &padded.request;
    request->query.AtaFlags = ATA_FLAGS_DATA_IN;
    request->query.CurrentTaskFile[6] = ID_CMD;
    request->query.DataBufferOffset = sizeof(request->query);
    request->query.DataTransferLength = sizeof(request->result);
    request->query.Length = sizeof(request->query);
    request->query.TimeOutValue = 10;

    DWORD* bytes_returned = &padded.bytes_returned;

    BOOL success = DeviceIoControl(
        volume.GetPlatformFile(), IOCTL_ATA_PASS_THROUGH, request,
        sizeof(*request), request, sizeof(*request), bytes_returned, NULL);

    if (success == FALSE || *bytes_returned < sizeof(*request) ||
        request->query.CurrentTaskFile[0]) {
      return false;
    }

    // Detect device driver writes further than request + bad driver space.
    // http://crbug.com/514822
    CHECK_LE(*bytes_returned, sizeof(padded) - sizeof(*bytes_returned));

    *has_seek_penalty = request->result.NominalMediaRotationRate != 1;
  }

  return true;
}

}  // namespace metrics

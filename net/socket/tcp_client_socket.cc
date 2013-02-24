// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket.h"

#include "base/file_util.h"
#include "base/files/file_path.h"

namespace net {

namespace {

#if defined(OS_LINUX)

// Checks to see if the system supports TCP FastOpen. Notably, it requires
// kernel support. Additionally, this checks system configuration to ensure that
// it's enabled.
bool SystemSupportsTCPFastOpen() {
  static const base::FilePath::CharType kTCPFastOpenProcFilePath[] =
      "/proc/sys/net/ipv4/tcp_fastopen";
  std::string system_enabled_tcp_fastopen;
  if (!file_util::ReadFileToString(
          base::FilePath(kTCPFastOpenProcFilePath),
          &system_enabled_tcp_fastopen)) {
    return false;
  }

  // As per http://lxr.linux.no/linux+v3.7.7/include/net/tcp.h#L225
  // TFO_CLIENT_ENABLE is the LSB
  if (system_enabled_tcp_fastopen.empty() ||
      (system_enabled_tcp_fastopen[0] & 0x1) == 0) {
    return false;
  }

  return true;
}

#else

bool SystemSupportsTCPFastOpen() {
  return false;
}

#endif

}

static bool g_tcp_fastopen_enabled = false;

void SetTCPFastOpenEnabled(bool value) {
  g_tcp_fastopen_enabled = value && SystemSupportsTCPFastOpen();
}

bool IsTCPFastOpenEnabled() {
  return g_tcp_fastopen_enabled;
}

}  // namespace net

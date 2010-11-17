// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities shared by both Broker RPC client and server.

#include "ceee/ie/broker/broker_rpc_utils.h"

#include <algorithm>

#include "base/win_util.h"
#include "ceee/common/process_utils_win.h"

// Local interprocess communication only.
const wchar_t kRpcProtocol[] = L"ncalrpc";

std::wstring GetRpcEndPointAddress() {
  std::wstring end_point;
  bool running_as_admin = false;
  // CEEE running as regular user and CEEE running as elevated user will start
  // different broker processes. So make end points names different to connect
  // to appropriate one.
  process_utils_win::IsCurrentProcessUacElevated(&running_as_admin);
  if (running_as_admin)
    end_point += L"ADMIN-";
  std::wstring sid;
  win_util::GetUserSidString(&sid);
  end_point += sid;
  end_point += L"-B4630D08-4621-41A1-A8D0-F1E98DA460D6";
  // XP does not accept endpoints longer than 52.
  end_point.resize(std::min(52u, end_point.size()));
  return end_point;
}

void  __RPC_FAR * __RPC_USER midl_user_allocate(size_t len) {
  return malloc(len);
}

void __RPC_USER midl_user_free(void __RPC_FAR * ptr) {
  free(ptr);
}

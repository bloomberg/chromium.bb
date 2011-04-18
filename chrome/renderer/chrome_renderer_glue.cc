// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the Chrome-specific embedder's side of random webkit glue
// functions.

#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/render_messages.h"
#include "content/renderer/render_thread.h"
#include "webkit/glue/webkit_glue.h"

#if !defined(DISABLE_NACL)
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"
#endif

namespace webkit_glue {

void UserMetricsRecordAction(const std::string& action) {
  RenderThread::current()->Send(
      new ViewHostMsg_UserMetricsRecordAction(action));
}

std::string GetProductVersion() {
  chrome::VersionInfo version_info;
  std::string product("Chrome/");
  product += version_info.is_valid() ? version_info.Version()
                                     : "0.0.0.0";
  return product;
}

#if !defined(DISABLE_NACL)
bool LaunchSelLdr(const char* alleged_url, int socket_count, void* imc_handles,
                  void* nacl_process_handle, int* nacl_process_id) {
  std::vector<nacl::FileDescriptor> sockets;
  base::ProcessHandle nacl_process;
  if (!RenderThread::current()->Send(
    new ViewHostMsg_LaunchNaCl(
        ASCIIToWide(alleged_url),
        socket_count,
        &sockets,
        &nacl_process,
        reinterpret_cast<base::ProcessId*>(nacl_process_id)))) {
    return false;
  }
  CHECK(static_cast<int>(sockets.size()) == socket_count);
  for (int i = 0; i < socket_count; i++) {
    static_cast<nacl::Handle*>(imc_handles)[i] =
        nacl::ToNativeHandle(sockets[i]);
  }
  *static_cast<nacl::Handle*>(nacl_process_handle) = nacl_process;
  return true;
}
#endif

}  // namespace webkit_glue

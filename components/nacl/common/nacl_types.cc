// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_types.h"
#include "ipc/ipc_platform_file.h"

namespace nacl {

NaClStartParams::NaClStartParams()
    : nexe_file(IPC::InvalidPlatformFileForTransit()),
      nexe_token_lo(0),
      nexe_token_hi(0),
      validation_cache_enabled(false),
      enable_exception_handling(false),
      enable_debug_stub(false),
      enable_ipc_proxy(false),
      uses_irt(false),
      enable_dyncode_syscalls(false) {
}

NaClStartParams::~NaClStartParams() {
}

NaClLaunchParams::NaClLaunchParams()
    : nexe_file(IPC::InvalidPlatformFileForTransit()),
      nexe_token_lo(0),
      nexe_token_hi(0),
      render_view_id(0),
      permission_bits(0),
      uses_irt(false),
      enable_dyncode_syscalls(false),
      enable_exception_handling(false),
      enable_crash_throttling(false) {
}

NaClLaunchParams::NaClLaunchParams(
    const std::string& manifest_url,
    const IPC::PlatformFileForTransit& nexe_file,
    uint64_t nexe_token_lo,
    uint64_t nexe_token_hi,
    int render_view_id,
    uint32 permission_bits,
    bool uses_irt,
    bool uses_nonsfi_mode,
    bool enable_dyncode_syscalls,
    bool enable_exception_handling,
    bool enable_crash_throttling)
    : manifest_url(manifest_url),
      nexe_file(nexe_file),
      nexe_token_lo(nexe_token_lo),
      nexe_token_hi(nexe_token_hi),
      render_view_id(render_view_id),
      permission_bits(permission_bits),
      uses_irt(uses_irt),
      uses_nonsfi_mode(uses_nonsfi_mode),
      enable_dyncode_syscalls(enable_dyncode_syscalls),
      enable_exception_handling(enable_exception_handling),
      enable_crash_throttling(enable_crash_throttling) {
}

NaClLaunchParams::~NaClLaunchParams() {
}

NaClLaunchResult::NaClLaunchResult()
    : imc_channel_handle(IPC::InvalidPlatformFileForTransit()),
      ppapi_ipc_channel_handle(),
      trusted_ipc_channel_handle(),
      plugin_pid(base::kNullProcessId),
      plugin_child_id(0) {
}

NaClLaunchResult::NaClLaunchResult(
    FileDescriptor imc_channel_handle,
    const IPC::ChannelHandle& ppapi_ipc_channel_handle,
    const IPC::ChannelHandle& trusted_ipc_channel_handle,
    const IPC::ChannelHandle& manifest_service_ipc_channel_handle,
    base::ProcessId plugin_pid,
    int plugin_child_id)
    : imc_channel_handle(imc_channel_handle),
      ppapi_ipc_channel_handle(ppapi_ipc_channel_handle),
      trusted_ipc_channel_handle(trusted_ipc_channel_handle),
      manifest_service_ipc_channel_handle(manifest_service_ipc_channel_handle),
      plugin_pid(plugin_pid),
      plugin_child_id(plugin_child_id) {
}

NaClLaunchResult::~NaClLaunchResult() {
}

}  // namespace nacl

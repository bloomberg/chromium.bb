// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_types.h"
#include "ipc/ipc_platform_file.h"

namespace nacl {

NaClStartParams::NaClStartParams()
    : nexe_file(IPC::InvalidPlatformFileForTransit()),
      imc_bootstrap_handle(IPC::InvalidPlatformFileForTransit()),
      irt_handle(IPC::InvalidPlatformFileForTransit()),
#if defined(OS_MACOSX)
      mac_shm_fd(IPC::InvalidPlatformFileForTransit()),
#endif
#if defined(OS_POSIX)
      debug_stub_server_bound_socket(IPC::InvalidPlatformFileForTransit()),
#endif
      validation_cache_enabled(false),
      enable_debug_stub(false),
      enable_ipc_proxy(false),
      process_type(kUnknownNaClProcessType),
      crash_info_shmem_handle(base::SharedMemory::NULLHandle()) {
}

NaClStartParams::~NaClStartParams() {
}

NaClResourcePrefetchResult::NaClResourcePrefetchResult()
    : file(IPC::InvalidPlatformFileForTransit()) {
}

NaClResourcePrefetchResult::NaClResourcePrefetchResult(
    const IPC::PlatformFileForTransit& file,
    const base::FilePath& file_path_metadata,
    const std::string& file_key)
    : file(file), file_path_metadata(file_path_metadata), file_key(file_key) {
}

NaClResourcePrefetchResult::~NaClResourcePrefetchResult() {
}

NaClResourcePrefetchRequest::NaClResourcePrefetchRequest() {
}

NaClResourcePrefetchRequest::NaClResourcePrefetchRequest(
    const std::string& file_key,
    const std::string& resource_url)
    : file_key(file_key),
      resource_url(resource_url) {
}

NaClResourcePrefetchRequest::~NaClResourcePrefetchRequest() {
}

NaClLaunchParams::NaClLaunchParams()
    : nexe_file(IPC::InvalidPlatformFileForTransit()),
      nexe_token_lo(0),
      nexe_token_hi(0),
      render_view_id(0),
      permission_bits(0),
      process_type(kUnknownNaClProcessType) {
}

NaClLaunchParams::NaClLaunchParams(
    const std::string& manifest_url,
    const IPC::PlatformFileForTransit& nexe_file,
    uint64_t nexe_token_lo,
    uint64_t nexe_token_hi,
    const std::vector<
        NaClResourcePrefetchRequest>& resource_prefetch_request_list,
    int render_view_id,
    uint32 permission_bits,
    bool uses_nonsfi_mode,
    NaClAppProcessType process_type)
    : manifest_url(manifest_url),
      nexe_file(nexe_file),
      nexe_token_lo(nexe_token_lo),
      nexe_token_hi(nexe_token_hi),
      resource_prefetch_request_list(resource_prefetch_request_list),
      render_view_id(render_view_id),
      permission_bits(permission_bits),
      uses_nonsfi_mode(uses_nonsfi_mode),
      process_type(process_type) {
}

NaClLaunchParams::~NaClLaunchParams() {
}

NaClLaunchResult::NaClLaunchResult()
    : imc_channel_handle(IPC::InvalidPlatformFileForTransit()),
      ppapi_ipc_channel_handle(),
      trusted_ipc_channel_handle(),
      plugin_pid(base::kNullProcessId),
      plugin_child_id(0),
      crash_info_shmem_handle(base::SharedMemory::NULLHandle()) {
}

NaClLaunchResult::NaClLaunchResult(
    const IPC::PlatformFileForTransit& imc_channel_handle,
    const IPC::ChannelHandle& ppapi_ipc_channel_handle,
    const IPC::ChannelHandle& trusted_ipc_channel_handle,
    const IPC::ChannelHandle& manifest_service_ipc_channel_handle,
    base::ProcessId plugin_pid,
    int plugin_child_id,
    base::SharedMemoryHandle crash_info_shmem_handle)
    : imc_channel_handle(imc_channel_handle),
      ppapi_ipc_channel_handle(ppapi_ipc_channel_handle),
      trusted_ipc_channel_handle(trusted_ipc_channel_handle),
      manifest_service_ipc_channel_handle(manifest_service_ipc_channel_handle),
      plugin_pid(plugin_pid),
      plugin_child_id(plugin_child_id),
      crash_info_shmem_handle(crash_info_shmem_handle) {
}

NaClLaunchResult::~NaClLaunchResult() {
}

}  // namespace nacl

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_TYPES_H_
#define COMPONENTS_NACL_COMMON_NACL_TYPES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_WIN)
#include <windows.h>   // for HANDLE
#endif

// TODO(gregoryd): add a Windows definition for base::FileDescriptor
namespace nacl {

#if defined(OS_WIN)
typedef HANDLE FileDescriptor;
inline HANDLE ToNativeHandle(const FileDescriptor& desc) {
  return desc;
}
#elif defined(OS_POSIX)
typedef base::FileDescriptor FileDescriptor;
inline int ToNativeHandle(const FileDescriptor& desc) {
  return desc.fd;
}
#endif


// Parameters sent to the NaCl process when we start it.
//
// If you change this, you will also need to update the IPC serialization in
// nacl_messages.h.
struct NaClStartParams {
  NaClStartParams();
  ~NaClStartParams();

  std::vector<FileDescriptor> handles;
  FileDescriptor debug_stub_server_bound_socket;

  bool validation_cache_enabled;
  std::string validation_cache_key;
  // Chrome version string. Sending the version string over IPC avoids linkage
  // issues in cases where NaCl is not compiled into the main Chromium
  // executable or DLL.
  std::string version;

  bool enable_exception_handling;
  bool enable_debug_stub;
  bool enable_ipc_proxy;
  bool uses_irt;
  bool enable_dyncode_syscalls;
};

// Parameters sent to the browser process to have it launch a NaCl process.
//
// If you change this, you will also need to update the IPC serialization in
// nacl_host_messages.h.
struct NaClLaunchParams {
  NaClLaunchParams();
  NaClLaunchParams(const std::string& u, int r, uint32 p,
                   bool uses_irt,
                   bool uses_nonsfi_mode,
                   bool enable_dyncode_syscalls,
                   bool enable_exception_handling,
                   bool enable_crash_throttling);
  NaClLaunchParams(const NaClLaunchParams& l);
  ~NaClLaunchParams();

  std::string manifest_url;
  int render_view_id;
  uint32 permission_bits;
  bool uses_irt;
  bool uses_nonsfi_mode;
  bool enable_dyncode_syscalls;
  bool enable_exception_handling;
  bool enable_crash_throttling;
};

struct NaClLaunchResult {
  NaClLaunchResult();
  NaClLaunchResult(FileDescriptor imc_channel_handle,
                   const IPC::ChannelHandle& ppapi_ipc_channel_handle,
                   const IPC::ChannelHandle& trusted_ipc_channel_handle,
                   base::ProcessId plugin_pid,
                   int plugin_child_id);
  ~NaClLaunchResult();

  // For plugin loader <-> renderer IMC communication.
  FileDescriptor imc_channel_handle;

  // For plugin <-> renderer PPAPI communication.
  IPC::ChannelHandle ppapi_ipc_channel_handle;

  // For plugin loader <-> renderer control communication (loading and
  // starting nexe).
  IPC::ChannelHandle trusted_ipc_channel_handle;
  base::ProcessId plugin_pid;
  int plugin_child_id;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_COMMON_NACL_TYPES_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and NaCl process.

// Multiply-included message file, no traditional include guard.
#include "base/process/process.h"
#include "components/nacl/common/nacl_types.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START NaClMsgStart

IPC_STRUCT_TRAITS_BEGIN(nacl::NaClStartParams)
  IPC_STRUCT_TRAITS_MEMBER(nexe_file)
  IPC_STRUCT_TRAITS_MEMBER(handles)
  IPC_STRUCT_TRAITS_MEMBER(debug_stub_server_bound_socket)
  IPC_STRUCT_TRAITS_MEMBER(validation_cache_enabled)
  IPC_STRUCT_TRAITS_MEMBER(validation_cache_key)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(enable_exception_handling)
  IPC_STRUCT_TRAITS_MEMBER(enable_debug_stub)
  IPC_STRUCT_TRAITS_MEMBER(enable_ipc_proxy)
  IPC_STRUCT_TRAITS_MEMBER(uses_irt)
  IPC_STRUCT_TRAITS_MEMBER(enable_dyncode_syscalls)
IPC_STRUCT_TRAITS_END()

//-----------------------------------------------------------------------------
// NaClProcess messages
// These are messages sent between the browser and the NaCl process.
// Tells the NaCl process to start.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_Start,
                     nacl::NaClStartParams /* params */)

#if defined(OS_WIN)
// Tells the NaCl broker to launch a NaCl loader process.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_LaunchLoaderThroughBroker,
                     std::string /* channel ID for the loader */)

// Notify the browser process that the loader was launched successfully.
IPC_MESSAGE_CONTROL2(NaClProcessMsg_LoaderLaunched,
                     std::string,  /* channel ID for the loader */
                     base::ProcessHandle /* loader process handle */)

// Tells the NaCl broker to attach a debug exception handler to the
// given NaCl loader process.
IPC_MESSAGE_CONTROL3(NaClProcessMsg_LaunchDebugExceptionHandler,
                     int32 /* pid of the NaCl process */,
                     base::ProcessHandle /* handle of the NaCl process */,
                     std::string /* NaCl internal process layout info */)

// Notify the browser process that the broker process finished
// attaching a debug exception handler to the given NaCl loader
// process.
IPC_MESSAGE_CONTROL2(NaClProcessMsg_DebugExceptionHandlerLaunched,
                     int32 /* pid */,
                     bool /* success */)

// Notify the broker that all loader processes have been terminated and it
// should shutdown.
IPC_MESSAGE_CONTROL0(NaClProcessMsg_StopBroker)

// Used by the NaCl process to request that a Windows debug exception
// handler be attached to it.
IPC_SYNC_MESSAGE_CONTROL1_1(NaClProcessMsg_AttachDebugExceptionHandler,
                            std::string, /* Internal process info */
                            bool /* Result */)

// Notify the browser process that the NaCl process has bound the given
// TCP port number to use for the GDB debug stub.
IPC_MESSAGE_CONTROL1(NaClProcessHostMsg_DebugStubPortSelected,
                     uint16_t /* debug_stub_port */)
#endif

// Used by the NaCl process to query a database in the browser.  The database
// contains the signatures of previously validated code chunks.
IPC_SYNC_MESSAGE_CONTROL1_1(NaClProcessMsg_QueryKnownToValidate,
                            std::string, /* A validation signature */
                            bool /* Can validation be skipped? */)

// Used by the NaCl process to add a validation signature to the validation
// database in the browser.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_SetKnownToValidate,
                     std::string /* A validation signature */)

// Used by the NaCl process to acquire trusted information about a file directly
// from the browser, including the file's path as well as a fresh version of the
// file handle.
IPC_SYNC_MESSAGE_CONTROL2_2(NaClProcessMsg_ResolveFileToken,
                            uint64, /* file_token_lo */
                            uint64, /* file_token_hi */
                            IPC::PlatformFileForTransit, /* fd */
                            base::FilePath /* Path opened to get fd */)

// Notify the browser process that the server side of the PPAPI channel was
// created successfully.
IPC_MESSAGE_CONTROL4(NaClProcessHostMsg_PpapiChannelsCreated,
                     IPC::ChannelHandle, /* browser_channel_handle */
                     IPC::ChannelHandle, /* ppapi_renderer_channel_handle */
                     IPC::ChannelHandle, /* trusted_renderer_channel_handle */
                     IPC::ChannelHandle /* manifest_service_channel_handle */)

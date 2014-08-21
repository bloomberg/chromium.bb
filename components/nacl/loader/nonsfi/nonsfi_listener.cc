// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/nonsfi_listener.h"

#include "base/command_line.h"
#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/loader/nacl_trusted_listener.h"
#include "components/nacl/loader/nonsfi/irt_random.h"
#include "components/nacl/loader/nonsfi/nonsfi_main.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/nacl_irt/plugin_startup.h"

#if !defined(OS_LINUX)
# error "non-SFI mode is supported only on linux."
#endif

namespace nacl {
namespace nonsfi {

NonSfiListener::NonSfiListener() : io_thread_("NaCl_IOThread"),
                                   shutdown_event_(true, false) {
  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
}

NonSfiListener::~NonSfiListener() {
}

void NonSfiListener::Listen() {
  channel_ = IPC::SyncChannel::Create(
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID),
      IPC::Channel::MODE_CLIENT,
      this,  // As a Listener.
      io_thread_.message_loop_proxy().get(),
      true,  // Create pipe now.
      &shutdown_event_);
  base::MessageLoop::current()->Run();
}

bool NonSfiListener::Send(IPC::Message* msg) {
  DCHECK(channel_.get() != NULL);
  return channel_->Send(msg);
}

bool NonSfiListener::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NonSfiListener, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_Start, OnStart)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NonSfiListener::OnStart(const nacl::NaClStartParams& params) {
  // Random number source initialization.
  SetUrandomFd(base::GetUrandomFD());

  IPC::ChannelHandle browser_handle;
  IPC::ChannelHandle ppapi_renderer_handle;
  IPC::ChannelHandle manifest_service_handle;

  if (params.enable_ipc_proxy) {
    browser_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");
    ppapi_renderer_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");
    manifest_service_handle =
        IPC::Channel::GenerateVerifiedChannelID("nacl");

    // In non-SFI mode, we neither intercept nor rewrite the message using
    // NaClIPCAdapter, and the channels are connected between the plugin and
    // the hosts directly. So, the IPC::Channel instances will be created in
    // the plugin side, because the IPC::Listener needs to live on the
    // plugin's main thread. However, on initialization (i.e. before loading
    // the plugin binary), the FD needs to be passed to the hosts. So, here
    // we create raw FD pairs, and pass the client side FDs to the hosts,
    // and the server side FDs to the plugin.
    int browser_server_ppapi_fd;
    int browser_client_ppapi_fd;
    int renderer_server_ppapi_fd;
    int renderer_client_ppapi_fd;
    int manifest_service_server_fd;
    int manifest_service_client_fd;
    if (!IPC::SocketPair(
            &browser_server_ppapi_fd, &browser_client_ppapi_fd) ||
        !IPC::SocketPair(
            &renderer_server_ppapi_fd, &renderer_client_ppapi_fd) ||
        !IPC::SocketPair(
            &manifest_service_server_fd, &manifest_service_client_fd)) {
      LOG(ERROR) << "Failed to create sockets for IPC.";
      return;
    }

    // Set the plugin IPC channel FDs.
    ppapi::SetIPCFileDescriptors(browser_server_ppapi_fd,
                                 renderer_server_ppapi_fd,
                                 manifest_service_server_fd);
    ppapi::StartUpPlugin();

    // Send back to the client side IPC channel FD to the host.
    browser_handle.socket =
        base::FileDescriptor(browser_client_ppapi_fd, true);
    ppapi_renderer_handle.socket =
        base::FileDescriptor(renderer_client_ppapi_fd, true);
    manifest_service_handle.socket =
        base::FileDescriptor(manifest_service_client_fd, true);
  }

  // TODO(teravest): Do we plan on using this renderer handle for nexe loading
  // for non-SFI? Right now, passing an empty channel handle instead causes
  // hangs, so we'll keep it.
  trusted_listener_ = new NaClTrustedListener(
      IPC::Channel::GenerateVerifiedChannelID("nacl"),
      io_thread_.message_loop_proxy().get(),
      &shutdown_event_);
  if (!Send(new NaClProcessHostMsg_PpapiChannelsCreated(
          browser_handle,
          ppapi_renderer_handle,
          trusted_listener_->TakeClientChannelHandle(),
          manifest_service_handle)))
    LOG(ERROR) << "Failed to send IPC channel handle to NaClProcessHost.";

  // Ensure that the validation cache key (used as an extra input to the
  // validation cache's hashing) isn't exposed accidentally.
  CHECK(!params.validation_cache_enabled);
  CHECK(params.validation_cache_key.size() == 0);
  CHECK(params.version.size() == 0);
  // Ensure that a debug stub FD isn't passed through accidentally.
  CHECK(!params.enable_debug_stub);
  CHECK(params.debug_stub_server_bound_socket.fd == -1);

  CHECK(!params.uses_irt);
  CHECK(params.handles.empty());

  CHECK(params.nexe_file != IPC::InvalidPlatformFileForTransit());
  CHECK(params.nexe_token_lo == 0);
  CHECK(params.nexe_token_hi == 0);
  MainStart(IPC::PlatformFileForTransitToPlatformFile(params.nexe_file));
}

}  // namespace nonsfi
}  // namespace nacl

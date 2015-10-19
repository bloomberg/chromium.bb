// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/nonsfi_listener.h"

#include "base/command_line.h"
#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/loader/nacl_trusted_listener.h"
#include "components/nacl/loader/nonsfi/nonsfi_main.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "native_client/src/public/nonsfi/irt_random.h"
#include "ppapi/nacl_irt/irt_manifest.h"
#include "ppapi/nacl_irt/plugin_startup.h"

#if !defined(OS_NACL_NONSFI)
#error "This file must be built for nacl_helper_nonsfi."
#endif

namespace nacl {
namespace nonsfi {

NonSfiListener::NonSfiListener() : io_thread_("NaCl_IOThread"),
                                   shutdown_event_(true, false),
                                   key_fd_map_(new std::map<std::string, int>) {
  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
}

NonSfiListener::~NonSfiListener() {
}

void NonSfiListener::Listen() {
  channel_ = IPC::SyncChannel::Create(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID),
      IPC::Channel::MODE_CLIENT,
      this,  // As a Listener.
      io_thread_.task_runner().get(),
      true,  // Create pipe now.
      &shutdown_event_);
  base::MessageLoop::current()->Run();
}

bool NonSfiListener::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NonSfiListener, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_AddPrefetchedResource,
                          OnAddPrefetchedResource)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_Start, OnStart)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NonSfiListener::OnAddPrefetchedResource(
    const nacl::NaClResourcePrefetchResult& prefetched_resource_file) {
  CHECK(prefetched_resource_file.file_path_metadata.empty());
  bool result = key_fd_map_->insert(std::make_pair(
      prefetched_resource_file.file_key,
      IPC::PlatformFileForTransitToPlatformFile(
          prefetched_resource_file.file))).second;
  if (!result) {
    LOG(FATAL) << "Duplicated open_resource key: "
               << prefetched_resource_file.file_key;
  }
}

void NonSfiListener::OnStart(const nacl::NaClStartParams& params) {
  // Random number source initialization.
  nonsfi_set_urandom_fd(base::GetUrandomFD());

  // In Non-SFI mode, PPAPI proxy must be enabled.
  CHECK(params.enable_ipc_proxy);

  // In Non-SFI mode, we neither intercept nor rewrite the message using
  // NaClIPCAdapter, and the channels are connected between the plugin and
  // the hosts directly. So, the IPC::Channel instances will be created in
  // the plugin side, because the IPC::Listener needs to live on the
  // plugin's main thread. We just pass the FDs to plugin side.
  // The FDs are created in the browser process. Following check can fail
  // if the preparation for sending NaClProcessMsg_Start were incomplete.
  CHECK_NE(params.ppapi_browser_channel_handle.socket.fd, -1);
  CHECK_NE(params.ppapi_renderer_channel_handle.socket.fd, -1);
  CHECK_NE(params.trusted_service_channel_handle.socket.fd, -1);
  CHECK_NE(params.manifest_service_channel_handle.socket.fd, -1);

  ppapi::SetIPCFileDescriptors(
      params.ppapi_browser_channel_handle.socket.fd,
      params.ppapi_renderer_channel_handle.socket.fd,
      params.manifest_service_channel_handle.socket.fd);
  ppapi::StartUpPlugin();

  // TODO(teravest): Do we plan on using this renderer handle for nexe loading
  // for non-SFI? Right now, passing an empty channel handle instead causes
  // hangs, so we'll keep it.
  trusted_listener_ =
      new NaClTrustedListener(params.trusted_service_channel_handle,
                              io_thread_.task_runner().get(), &shutdown_event_);

  // Ensure that the validation cache key (used as an extra input to the
  // validation cache's hashing) isn't exposed accidentally.
  CHECK(!params.validation_cache_enabled);
  CHECK(params.validation_cache_key.size() == 0);
  CHECK(params.version.size() == 0);
  // Ensure that a debug stub FD isn't passed through accidentally.
  CHECK(!params.enable_debug_stub);
  CHECK(params.debug_stub_server_bound_socket.fd == -1);

  CHECK(params.imc_bootstrap_handle == IPC::InvalidPlatformFileForTransit());
  CHECK(params.irt_handle == IPC::InvalidPlatformFileForTransit());
  CHECK(params.debug_stub_server_bound_socket ==
        IPC::InvalidPlatformFileForTransit());

  // We are only expecting non-SFI mode to be used with NaCl for now,
  // not PNaCl processes.
  CHECK(params.process_type == kNativeNaClProcessType);

  CHECK(params.nexe_file != IPC::InvalidPlatformFileForTransit());
  CHECK(params.nexe_file_path_metadata.empty());

  ppapi::RegisterPreopenedDescriptorsNonSfi(key_fd_map_.release());

  MainStart(IPC::PlatformFileForTransitToPlatformFile(params.nexe_file));
}

}  // namespace nonsfi
}  // namespace nacl

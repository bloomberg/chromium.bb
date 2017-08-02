// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_sender.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/constants.mojom.h"
#include "chrome/common/profiling/memlog.mojom.h"
#include "chrome/common/profiling/memlog_allocator_shim.h"
#include "chrome/common/profiling/memlog_sender_pipe.h"
#include "chrome/common/profiling/memlog_stream.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "services/service_manager/public/cpp/connector.h"

namespace profiling {

namespace {

// TODO(brettw) this is a hack to allow StartProfilingMojo to work. Figure out
// how to get the lifetime of this that allows that function call to work.
MemlogSenderPipe* memlog_sender_pipe = nullptr;

}  // namespace

void InitMemlogSenderIfNecessary(
    content::ServiceManagerConnection* connection) {
  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  // TODO(ajwong): Rename to kMemlogId.
  std::string pipe_id_str = cmdline.GetSwitchValueASCII(switches::kMemlogPipe);
  if (pipe_id_str.empty()) {
    return;
  }
  int sender_id;
  CHECK(base::StringToInt(pipe_id_str, &sender_id));

  // The |memlog| interface is used for a one-shot, no-response, publication
  // of the filehandle for the data channel. As such, there is no need to have
  // the MemlogPtr live beyond the AddSender() call.
  mojom::MemlogPtr memlog;
  connection->GetConnector()->BindInterface(profiling::mojom::kServiceName,
                                            &memlog);

  mojo::edk::PlatformChannelPair data_channel;
  memlog->AddSender(
      mojo::WrapPlatformFile(data_channel.PassServerHandle().release().handle),
      sender_id);
  StartMemlogSender(base::ScopedPlatformFile(
      data_channel.PassClientHandle().release().handle));
}

void StartMemlogSender(base::ScopedPlatformFile file) {
  static MemlogSenderPipe pipe(std::move(file));
  memlog_sender_pipe = &pipe;

  StreamHeader header;
  header.signature = kStreamSignature;
  pipe.Send(&header, sizeof(StreamHeader));

  InitAllocatorShim(&pipe);
}

void StartProfilingMojo() {
  static bool started_mojo = false;

  if (!started_mojo) {
    started_mojo = true;
    StartMojoControlPacket start_mojo_message;
    start_mojo_message.op = kStartMojoControlPacketType;
    memlog_sender_pipe->Send(&start_mojo_message, sizeof(start_mojo_message));
  }
}

}  // namespace profiling

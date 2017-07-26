// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_process.h"

#include "base/bind.h"
#include "base/files/platform_file.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "chrome/profiling/memlog_receiver_pipe.h"
#include "chrome/profiling/profiling_globals.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace profiling {

ProfilingProcess::ProfilingProcess() : binding_(this) {}

ProfilingProcess::~ProfilingProcess() {}

void ProfilingProcess::EnsureMojoStarted() {
  if (started_mojo_)
    return;
  started_mojo_ = true;

  control_invitation_ =
      mojo::edk::IncomingBrokerClientInvitation::AcceptFromCommandLine(
          mojo::edk::TransportProtocol::kLegacy);

  binding_.Bind(mojom::ProfilingControlRequest(
      control_invitation_->ExtractMessagePipe(kProfilingControlPipeName)));
}

void ProfilingProcess::AddNewSender(mojo::ScopedHandle sender_pipe,
                                    int32_t sender_pid) {
  base::PlatformFile sender_file;
  MojoResult result =
      mojo::UnwrapPlatformFile(std::move(sender_pipe), &sender_file);
  CHECK_EQ(result, MOJO_RESULT_OK);

  ProfilingGlobals::Get()->GetMemlogConnectionManager()->OnNewConnection(
      scoped_refptr<MemlogReceiverPipe>(new MemlogReceiverPipe(sender_file)),
      sender_pid);
}

void ProfilingProcess::DumpProcess(int32_t pid) {}

}  // namespace profiling

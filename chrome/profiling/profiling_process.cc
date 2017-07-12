// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_process.h"

#include "base/bind.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "chrome/profiling/profiling_globals.h"

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
                                    int32_t sender_pid) {}

}  // namespace profiling

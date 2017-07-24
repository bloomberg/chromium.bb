// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_PROFILING_PROCESS_H_
#define CHROME_PROFILING_PROFILING_PROCESS_H_

#include "base/macros.h"
#include "chrome/common/profiling/profiling_control.mojom.h"
#include "mojo/edk/embedder/incoming_broker_client_invitation.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace profiling {

class MemlogReceiverPipeServer;

// Represents the profiling process side of the profiling <-> browser
// connection. This class is not thread safe and must onle be called on the IO
// thread (which is the main thread in the profiling process).
class ProfilingProcess : public mojom::ProfilingControl {
 public:
  ProfilingProcess();
  ~ProfilingProcess() override;

  void EnsureMojoStarted();
  void AttachPipeServer(scoped_refptr<MemlogReceiverPipeServer> server);

  // ProfilingControl implementation.
  void AddNewSender(mojo::ScopedHandle sender_pipe,
                    int32_t sender_pid) override;
  void DumpProcess(int32_t pid) override;

 private:
  bool started_mojo_ = false;

  std::unique_ptr<mojo::edk::IncomingBrokerClientInvitation>
      control_invitation_;

  scoped_refptr<MemlogReceiverPipeServer> server_;
  mojo::Binding<mojom::ProfilingControl> binding_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingProcess);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_PROFILING_PROCESS_H_

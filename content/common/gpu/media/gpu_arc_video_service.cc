// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gpu_arc_video_service.h"

#include "base/logging.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sync_channel.h"

namespace content {

// TODO(kcwu) implement ArcVideoAccelerator::Client.
class GpuArcVideoService::AcceleratorStub : public IPC::Listener,
                                            public IPC::Sender {
 public:
  // |owner| outlives AcceleratorStub.
  explicit AcceleratorStub(GpuArcVideoService* owner) : owner_(owner) {}

  ~AcceleratorStub() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    channel_->Close();
  }

  IPC::ChannelHandle CreateChannel(
      base::WaitableEvent* shutdown_event,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner) {
    IPC::ChannelHandle handle =
        IPC::Channel::GenerateVerifiedChannelID("arc-video");
    channel_ = IPC::SyncChannel::Create(handle, IPC::Channel::MODE_SERVER, this,
                                        io_task_runner, false, shutdown_event);
    base::ScopedFD client_fd = channel_->TakeClientFileDescriptor();
    DCHECK(client_fd.is_valid());
    handle.socket = base::FileDescriptor(std::move(client_fd));
    return handle;
  }

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override {
    DCHECK(msg);
    return channel_->Send(msg);
  }

  // IPC::Listener implementation:
  void OnChannelError() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    // RemoveClient will delete |this|.
    owner_->RemoveClient(this);
  }

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    // TODO(kcwu) Add handlers here.
    return false;
  }

 private:
  base::ThreadChecker thread_checker_;
  GpuArcVideoService* const owner_;
  scoped_ptr<IPC::SyncChannel> channel_;
};

GpuArcVideoService::GpuArcVideoService(
    base::WaitableEvent* shutdown_event,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : shutdown_event_(shutdown_event), io_task_runner_(io_task_runner) {}

GpuArcVideoService::~GpuArcVideoService() {}

void GpuArcVideoService::CreateChannel(const CreateChannelCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<AcceleratorStub> stub(new AcceleratorStub(this));

  IPC::ChannelHandle handle =
      stub->CreateChannel(shutdown_event_, io_task_runner_);
  accelerator_stubs_[stub.get()] = std::move(stub);

  callback.Run(handle);
}

void GpuArcVideoService::RemoveClient(AcceleratorStub* stub) {
  DCHECK(thread_checker_.CalledOnValidThread());

  accelerator_stubs_.erase(stub);
}

}  // namespace content

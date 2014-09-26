// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo_host.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "ipc/mojo/ipc_channel_mojo.h"

namespace IPC {

// The delete class lives on the IO thread to talk to ChannelMojo on
// behalf of ChannelMojoHost.
//
// The object must be touched only on the IO thread.
class ChannelMojoHost::ChannelDelegate : public ChannelMojo::Delegate {
 public:
  explicit ChannelDelegate(scoped_refptr<base::TaskRunner> io_task_runner);
  virtual ~ChannelDelegate();

  // ChannelMojo::Delegate
  virtual base::WeakPtr<Delegate> ToWeakPtr() OVERRIDE;
  virtual void OnChannelCreated(base::WeakPtr<ChannelMojo> channel) OVERRIDE;
  virtual scoped_refptr<base::TaskRunner> GetIOTaskRunner() OVERRIDE;

  // Returns an weak ptr of ChannelDelegate instead of Delegate
  base::WeakPtr<ChannelDelegate> GetWeakPtr();
  void OnClientLaunched(base::ProcessHandle process);
  void DeleteThisSoon();

 private:
  scoped_refptr<base::TaskRunner> io_task_runner_;
  base::WeakPtrFactory<ChannelDelegate> weak_factory_;
  base::WeakPtr<ChannelMojo> channel_;

  DISALLOW_COPY_AND_ASSIGN(ChannelDelegate);
};

ChannelMojoHost::ChannelDelegate::ChannelDelegate(
    scoped_refptr<base::TaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner), weak_factory_(this) {
}

ChannelMojoHost::ChannelDelegate::~ChannelDelegate() {
}

base::WeakPtr<ChannelMojo::Delegate>
ChannelMojoHost::ChannelDelegate::ToWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<ChannelMojoHost::ChannelDelegate>
ChannelMojoHost::ChannelDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ChannelMojoHost::ChannelDelegate::OnChannelCreated(
    base::WeakPtr<ChannelMojo> channel) {
  DCHECK(!channel_);
  channel_ = channel;
}

scoped_refptr<base::TaskRunner>
ChannelMojoHost::ChannelDelegate::GetIOTaskRunner() {
  return io_task_runner_;
}

void ChannelMojoHost::ChannelDelegate::OnClientLaunched(
    base::ProcessHandle process) {
  if (channel_)
    channel_->OnClientLaunched(process);
}

void ChannelMojoHost::ChannelDelegate::DeleteThisSoon() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&base::DeletePointer<ChannelMojoHost::ChannelDelegate>,
                 base::Unretained(this)));
}

//
// ChannelMojoHost
//

ChannelMojoHost::ChannelMojoHost(scoped_refptr<base::TaskRunner> io_task_runner)
    : weak_factory_(this),
      io_task_runner_(io_task_runner),
      channel_delegate_(new ChannelDelegate(io_task_runner)) {
}

ChannelMojoHost::~ChannelMojoHost() {
}

void ChannelMojoHost::OnClientLaunched(base::ProcessHandle process) {
  if (io_task_runner_ == base::MessageLoop::current()->message_loop_proxy()) {
    channel_delegate_->OnClientLaunched(process);
  } else {
    io_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&ChannelDelegate::OnClientLaunched,
                                         channel_delegate_->GetWeakPtr(),
                                         process));
  }
}

ChannelMojo::Delegate* ChannelMojoHost::channel_delegate() const {
  return channel_delegate_.get();
}

void ChannelMojoHost::DelegateDeleter::operator()(
    ChannelMojoHost::ChannelDelegate* ptr) const {
  ptr->DeleteThisSoon();
}

}  // namespace IPC

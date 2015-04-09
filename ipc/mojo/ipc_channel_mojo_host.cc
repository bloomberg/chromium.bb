// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo_host.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "ipc/mojo/ipc_channel_mojo.h"

namespace IPC {

class ChannelMojoHost::ChannelDelegateTraits {
 public:
  static void Destruct(const ChannelMojoHost::ChannelDelegate* ptr);
};

// The delete class lives on the IO thread to talk to ChannelMojo on
// behalf of ChannelMojoHost.
//
// The object must be touched only on the IO thread.
class ChannelMojoHost::ChannelDelegate
    : public base::RefCountedThreadSafe<ChannelMojoHost::ChannelDelegate,
                                        ChannelMojoHost::ChannelDelegateTraits>,
      public ChannelMojo::Delegate {
 public:
  explicit ChannelDelegate(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // ChannelMojo::Delegate
  base::WeakPtr<Delegate> ToWeakPtr() override;
  void OnChannelCreated(base::WeakPtr<ChannelMojo> channel) override;
  scoped_refptr<base::TaskRunner> GetIOTaskRunner() override;

  // Returns an weak ptr of ChannelDelegate instead of Delegate
  base::WeakPtr<ChannelDelegate> GetWeakPtr();
  void OnClientLaunched(base::ProcessHandle process);
  void DeleteThisSoon() const;

 private:
  friend class base::DeleteHelper<ChannelDelegate>;

  ~ChannelDelegate() override;

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  base::WeakPtr<ChannelMojo> channel_;
  base::WeakPtrFactory<ChannelDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelDelegate);
};

ChannelMojoHost::ChannelDelegate::ChannelDelegate(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
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

void ChannelMojoHost::ChannelDelegate::DeleteThisSoon() const {
  io_task_runner_->DeleteSoon(FROM_HERE, this);
}

//
// ChannelMojoHost
//

ChannelMojoHost::ChannelMojoHost(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner),
      channel_delegate_(new ChannelDelegate(io_task_runner)),
      weak_factory_(this) {
}

ChannelMojoHost::~ChannelMojoHost() {
}

void ChannelMojoHost::OnClientLaunched(base::ProcessHandle process) {
  if (io_task_runner_ == base::MessageLoop::current()->message_loop_proxy()) {
    channel_delegate_->OnClientLaunched(process);
  } else {
    io_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&ChannelDelegate::OnClientLaunched,
                                         channel_delegate_, process));
  }
}

ChannelMojo::Delegate* ChannelMojoHost::channel_delegate() const {
  return channel_delegate_.get();
}

// static
void ChannelMojoHost::ChannelDelegateTraits::Destruct(
    const ChannelMojoHost::ChannelDelegate* ptr) {
  ptr->DeleteThisSoon();
}

}  // namespace IPC

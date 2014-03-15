// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/mojo_channel_init.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "mojo/system/embedder/embedder.h"

namespace content {

namespace {

struct Initializer {
  Initializer() {
    mojo::embedder::Init();
  }
};

static base::LazyInstance<Initializer>::Leaky initializer =
    LAZY_INSTANCE_INITIALIZER;

// Initializes mojo. Use a lazy instance to ensure we only do this once.
// TODO(sky): this likely wants to move to a more central location, such as
// startup.
void InitMojo() {
  initializer.Get();
}

}  // namespace

MojoChannelInit::MojoChannelInit()
    : main_thread_(base::MessageLoop::current()->message_loop_proxy()),
      channel_info_(NULL),
      weak_factory_(this) {
}

MojoChannelInit::~MojoChannelInit() {
  bootstrap_message_pipe_.reset();
  if (channel_info_) {
    io_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&mojo::embedder::DestroyChannelOnIOThread, channel_info_));
  }
}

void MojoChannelInit::Init(
    base::PlatformFile file,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  DCHECK(!io_thread_task_runner_.get());  // Should only init once.
  io_thread_task_runner_ = io_thread_task_runner;
  InitMojo();
  bootstrap_message_pipe_.reset(
      mojo::Handle(
          mojo::embedder::CreateChannel(
              mojo::embedder::ScopedPlatformHandle(
                  mojo::embedder::PlatformHandle(file)),
              io_thread_task_runner,
              base::Bind(&MojoChannelInit::OnCreatedChannelOnIOThread,
                         weak_factory_.GetWeakPtr(),
                         main_thread_,
                         io_thread_task_runner))));
}

// static
void MojoChannelInit::OnCreatedChannelOnIOThread(
    base::WeakPtr<MojoChannelInit> host,
    scoped_refptr<base::TaskRunner> main_thread,
    scoped_refptr<base::TaskRunner> io_thread,
    mojo::embedder::ChannelInfo* channel) {
  main_thread->PostTask(
      FROM_HERE,
      base::Bind(&MojoChannelInit::OnCreatedChannelOnMainThread, host,
                 io_thread, channel));
}

// static
void MojoChannelInit::OnCreatedChannelOnMainThread(
    base::WeakPtr<MojoChannelInit> host,
    scoped_refptr<base::TaskRunner> io_thread,
    mojo::embedder::ChannelInfo* channel) {
  // By the time we get here |host| may have been destroyed. If so, shutdown the
  // channel.
  if (!host.get()) {
    io_thread->PostTask(
        FROM_HERE,
        base::Bind(&mojo::embedder::DestroyChannelOnIOThread, channel));
    return;
  }
  host->channel_info_ = channel;
}


}  // namespace content

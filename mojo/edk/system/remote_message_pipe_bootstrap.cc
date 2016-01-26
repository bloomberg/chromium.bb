// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/remote_message_pipe_bootstrap.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/system/node_controller.h"

namespace mojo {
namespace edk {

namespace {

const size_t kMaxTokenSize = 256;

class ParentBootstrap : public RemoteMessagePipeBootstrap {
 public:
  static void Create(NodeController* node_controller,
                     ScopedPlatformHandle platform_handle,
                     const std::string& token) {
    // Owns itself.
    new ParentBootstrap(node_controller, std::move(platform_handle), token);
  }

 private:
  ParentBootstrap(NodeController* node_controller,
                  ScopedPlatformHandle platform_handle,
                  const std::string& token)
      : RemoteMessagePipeBootstrap(std::move(platform_handle)),
        node_controller_(node_controller),
        token_(token) {
    DCHECK_LE(token_.size(), kMaxTokenSize);
    Channel::MessagePtr message(new Channel::Message(token_.size(), 0));
    memcpy(message->mutable_payload(), token_.data(), token_.size());
    channel_->Write(std::move(message));
  }

  ~ParentBootstrap() override {}

  void OnTokenReceived(const std::string& token) override {
    // It's possible for two different endpoints in the same parent process to
    // initiate parent bootstrap on the same platform channel without being
    // aware of the other.
    //
    // This will successfully connect the two endpoints as long as at least one
    // of the ParentBootstrap instances receives the other's token. It's OK that
    // they race. NodeController will silently ignore any missed reservations.
    node_controller_->ConnectReservedPorts(token_, token);
    ShutDown();
  }

  NodeController* node_controller_;
  const std::string token_;

  DISALLOW_COPY_AND_ASSIGN(ParentBootstrap);
};

class ChildBootstrap : public RemoteMessagePipeBootstrap {
 public:
  static void Create(NodeController* node_controller,
                     ScopedPlatformHandle platform_handle,
                     const ports::PortRef& port,
                     const base::Closure& callback) {
    // Owns itself.
    new ChildBootstrap(
        node_controller, std::move(platform_handle), port, callback);
  }

 private:
  ChildBootstrap(NodeController* node_controller,
                 ScopedPlatformHandle platform_handle,
                 const ports::PortRef& port,
                 const base::Closure& callback)
      : RemoteMessagePipeBootstrap(std::move(platform_handle)),
        node_controller_(node_controller),
        port_(port),
        callback_(callback) {
  }

  ~ChildBootstrap() override {}

  void OnTokenReceived(const std::string& token) override {
    CHECK(!callback_.is_null());
    base::Closure callback = callback_;
    callback_.Reset();

    node_controller_->ConnectToParentPort(port_, token, callback);
    ShutDown();
  }

  NodeController* const node_controller_;
  const ports::PortRef port_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(ChildBootstrap);
};

}  // namespace

// static
void RemoteMessagePipeBootstrap::CreateForParent(
    NodeController* node_controller,
    ScopedPlatformHandle platform_handle,
    const std::string& token) {
  node_controller->io_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ParentBootstrap::Create, base::Unretained(node_controller),
                 base::Passed(&platform_handle), token));
}

// static
void RemoteMessagePipeBootstrap::CreateForChild(
    NodeController* node_controller,
    ScopedPlatformHandle platform_handle,
    const ports::PortRef& port,
    const base::Closure& callback) {
  node_controller->io_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChildBootstrap::Create, base::Unretained(node_controller),
                 base::Passed(&platform_handle), port, callback));
}

RemoteMessagePipeBootstrap::~RemoteMessagePipeBootstrap() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  base::MessageLoop::current()->RemoveDestructionObserver(this);
  if (channel_)
    channel_->ShutDown();
}

RemoteMessagePipeBootstrap::RemoteMessagePipeBootstrap(
    ScopedPlatformHandle platform_handle)
    : io_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      channel_(Channel::Create(this, std::move(platform_handle),
                               io_task_runner_)) {
  base::MessageLoop::current()->AddDestructionObserver(this);
  channel_->Start();
}

void RemoteMessagePipeBootstrap::ShutDown() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  if (shutting_down_)
    return;

  shutting_down_ = true;

  // Shut down asynchronously so implementations are free to call it whenever.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RemoteMessagePipeBootstrap::ShutDownNow,
                 base::Unretained(this)));
}

void RemoteMessagePipeBootstrap::WillDestroyCurrentMessageLoop() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  ShutDownNow();
}

void RemoteMessagePipeBootstrap::OnChannelMessage(
    const void* payload,
    size_t payload_size,
    ScopedPlatformHandleVectorPtr handles) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!handles || !handles->size());
  if (payload_size > kMaxTokenSize) {
    DLOG(ERROR) << "Invalid token payload. Dropping message pipe bootstrap.";
    ShutDown();
    return;
  }

  OnTokenReceived(std::string(static_cast<const char*>(payload), payload_size));
}

void RemoteMessagePipeBootstrap::OnChannelError() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  ShutDown();
}

void RemoteMessagePipeBootstrap::ShutDownNow() {
  delete this;
}

}  // namespace edk
}  // namespace mojo

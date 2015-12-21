// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/embedder.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/edk/system/broker_state.h"
#include "mojo/edk/system/child_broker.h"
#include "mojo/edk/system/child_broker_host.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/platform_handle_dispatcher.h"

namespace mojo {
namespace edk {

namespace internal {

// Declared in embedder_internal.h.
Broker* g_broker = nullptr;
PlatformSupport* g_platform_support = nullptr;
Core* g_core = nullptr;

ProcessDelegate* g_process_delegate;
base::TaskRunner* g_io_thread_task_runner = nullptr;

Core* GetCore() {
  return g_core;
}

}  // namespace internal

void SetMaxMessageSize(size_t bytes) {
  GetMutableConfiguration()->max_message_num_bytes = bytes;
}

void PreInitializeParentProcess() {
  BrokerState::GetInstance();
}

void PreInitializeChildProcess() {
  ChildBroker::GetInstance();
}

ScopedPlatformHandle ChildProcessLaunched(base::ProcessHandle child_process) {
  PlatformChannelPair token_channel;
  new ChildBrokerHost(child_process, token_channel.PassServerHandle());
  return token_channel.PassClientHandle();
}

void ChildProcessLaunched(base::ProcessHandle child_process,
                          ScopedPlatformHandle server_pipe) {
  new ChildBrokerHost(child_process, std::move(server_pipe));
}

void SetParentPipeHandle(ScopedPlatformHandle pipe) {
  ChildBroker::GetInstance()->SetChildBrokerHostHandle(std::move(pipe));
}

void Init() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch("use-new-edk") && !internal::g_broker)
    BrokerState::GetInstance();

  DCHECK(!internal::g_platform_support);
  internal::g_platform_support = new SimplePlatformSupport();

  DCHECK(!internal::g_core);
  internal::g_core = new Core(internal::g_platform_support);
}

MojoResult AsyncWait(MojoHandle handle,
                     MojoHandleSignals signals,
                     const base::Callback<void(MojoResult)>& callback) {
  return internal::g_core->AsyncWait(handle, signals, callback);
}

MojoResult CreatePlatformHandleWrapper(
    ScopedPlatformHandle platform_handle,
    MojoHandle* platform_handle_wrapper_handle) {
  DCHECK(platform_handle_wrapper_handle);

  scoped_refptr<Dispatcher> dispatcher =
      PlatformHandleDispatcher::Create(std::move(platform_handle));

  DCHECK(internal::g_core);
  MojoHandle h = internal::g_core->AddDispatcher(dispatcher);
  if (h == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *platform_handle_wrapper_handle = h;
  return MOJO_RESULT_OK;
}

MojoResult PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                                     ScopedPlatformHandle* platform_handle) {
  DCHECK(platform_handle);

  DCHECK(internal::g_core);
  scoped_refptr<Dispatcher> dispatcher(
      internal::g_core->GetDispatcher(platform_handle_wrapper_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (dispatcher->GetType() != Dispatcher::Type::PLATFORM_HANDLE)
    return MOJO_RESULT_INVALID_ARGUMENT;

  *platform_handle = static_cast<PlatformHandleDispatcher*>(dispatcher.get())
                         ->PassPlatformHandle();
  return MOJO_RESULT_OK;
}

void InitIPCSupport(ProcessDelegate* process_delegate,
                    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  // |Init()| must have already been called.
  DCHECK(internal::g_core);
  internal::g_process_delegate = process_delegate;
  internal::g_io_thread_task_runner = io_thread_task_runner.get();
}

void ShutdownIPCSupportOnIOThread() {
}

void ShutdownIPCSupport() {
  // TODO(jam): remove ProcessDelegate from new EDK once the old EDK is gone.
  internal::g_process_delegate->OnShutdownComplete();
}

ScopedMessagePipeHandle CreateMessagePipe(
    ScopedPlatformHandle platform_handle) {
  MojoCreateMessagePipeOptions options = {
      static_cast<uint32_t>(sizeof(MojoCreateMessagePipeOptions)),
      MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_TRANSFERABLE};
  scoped_refptr<MessagePipeDispatcher> dispatcher =
      MessagePipeDispatcher::Create(options);

  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));
  CHECK(rv.is_valid());
  dispatcher->Init(std::move(platform_handle), nullptr, 0, nullptr, 0, nullptr,
                   nullptr);
  // TODO(vtl): The |.Pass()| below is only needed due to an MSVS bug; remove it
  // once that's fixed.
  return rv;
}

}  // namespace edk
}  // namespace mojo

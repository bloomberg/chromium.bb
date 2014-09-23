// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "ipc/ipc_test_base.h"

#include "base/command_line.h"
#include "base/process/kill.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "ipc/ipc_descriptors.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#endif

// static
std::string IPCTestBase::GetChannelName(const std::string& test_client_name) {
  DCHECK(!test_client_name.empty());
  return test_client_name + "__Channel";
}

IPCTestBase::IPCTestBase()
    : client_process_(base::kNullProcessHandle) {
}

IPCTestBase::~IPCTestBase() {
}

void IPCTestBase::TearDown() {
  message_loop_.reset();
  MultiProcessTest::TearDown();
}

void IPCTestBase::Init(const std::string& test_client_name) {
  InitWithCustomMessageLoop(
      test_client_name,
      scoped_ptr<base::MessageLoop>(new base::MessageLoopForIO()));
}

void IPCTestBase::InitWithCustomMessageLoop(
    const std::string& test_client_name,
    scoped_ptr<base::MessageLoop> message_loop) {
  DCHECK(!test_client_name.empty());
  DCHECK(test_client_name_.empty());
  DCHECK(!message_loop_);

  test_client_name_ = test_client_name;
  message_loop_ = message_loop.Pass();
}

void IPCTestBase::CreateChannel(IPC::Listener* listener) {
  CreateChannelFromChannelHandle(GetTestChannelHandle(), listener);
}

bool IPCTestBase::ConnectChannel() {
  CHECK(channel_.get());
  return channel_->Connect();
}

scoped_ptr<IPC::Channel> IPCTestBase::ReleaseChannel() {
  return channel_.Pass();
}

void IPCTestBase::SetChannel(scoped_ptr<IPC::Channel> channel) {
  channel_ = channel.Pass();
}


void IPCTestBase::DestroyChannel() {
  DCHECK(channel_.get());
  channel_.reset();
}

void IPCTestBase::CreateChannelFromChannelHandle(
    const IPC::ChannelHandle& channel_handle,
    IPC::Listener* listener) {
  CHECK(!channel_.get());
  CHECK(!channel_proxy_.get());
  channel_ = CreateChannelFactory(
      channel_handle, task_runner().get())->BuildChannel(listener);
}

void IPCTestBase::CreateChannelProxy(
    IPC::Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  CHECK(!channel_.get());
  CHECK(!channel_proxy_.get());
  channel_proxy_ = IPC::ChannelProxy::Create(
      CreateChannelFactory(GetTestChannelHandle(), ipc_task_runner.get()),
      listener,
      ipc_task_runner);
}

void IPCTestBase::DestroyChannelProxy() {
  CHECK(channel_proxy_.get());
  channel_proxy_.reset();
}

std::string IPCTestBase::GetTestMainName() const {
  return test_client_name_ + "TestClientMain";
}

bool IPCTestBase::DidStartClient() {
  DCHECK_NE(base::kNullProcessHandle, client_process_);
  return client_process_ != base::kNullProcessHandle;
}

#if defined(OS_POSIX)

bool IPCTestBase::StartClient() {
  return StartClientWithFD(channel_
                               ? channel_->GetClientFileDescriptor()
                               : channel_proxy_->GetClientFileDescriptor());
}

bool IPCTestBase::StartClientWithFD(int ipcfd) {
  DCHECK_EQ(client_process_, base::kNullProcessHandle);

  base::FileHandleMappingVector fds_to_map;
  if (ipcfd > -1)
    fds_to_map.push_back(std::pair<int, int>(ipcfd,
        kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
  base::LaunchOptions options;
  options.fds_to_remap = &fds_to_map;
  client_process_ = SpawnChildWithOptions(GetTestMainName(), options);

  return DidStartClient();
}

#elif defined(OS_WIN)

bool IPCTestBase::StartClient() {
  DCHECK_EQ(client_process_, base::kNullProcessHandle);
  client_process_ = SpawnChild(GetTestMainName());
  return DidStartClient();
}

#endif

bool IPCTestBase::WaitForClientShutdown() {
  DCHECK(client_process_ != base::kNullProcessHandle);

  bool rv = base::WaitForSingleProcess(client_process_,
                                       base::TimeDelta::FromSeconds(5));
  base::CloseProcessHandle(client_process_);
  client_process_ = base::kNullProcessHandle;
  return rv;
}

IPC::ChannelHandle IPCTestBase::GetTestChannelHandle() {
  return GetChannelName(test_client_name_);
}

scoped_refptr<base::TaskRunner> IPCTestBase::task_runner() {
  return message_loop_->message_loop_proxy();
}

scoped_ptr<IPC::ChannelFactory> IPCTestBase::CreateChannelFactory(
    const IPC::ChannelHandle& handle,
    base::TaskRunner* runner) {
  return IPC::ChannelFactory::Create(handle, IPC::Channel::MODE_SERVER);
}

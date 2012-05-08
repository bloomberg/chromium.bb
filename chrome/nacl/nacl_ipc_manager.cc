// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_ipc_manager.h"

#include "chrome/nacl/nacl_ipc_adapter.h"
#include "content/common/child_process.h"

NaClIPCManager::NaClIPCManager() {
}

NaClIPCManager::~NaClIPCManager() {
}

void NaClIPCManager::Init(
    scoped_refptr<base::MessageLoopProxy> io_thread_proxy) {
  io_thread_proxy_ = io_thread_proxy;
}

void* NaClIPCManager::CreateChannel(const IPC::ChannelHandle& handle) {
  DCHECK(io_thread_proxy_.get());
  scoped_refptr<NaClIPCAdapter> adapter(
      new NaClIPCAdapter(handle, io_thread_proxy_.get()));

  // Use the object's address as the handle given to nacl. We just need a
  // unique void* to give to nacl for us to look it up when we get calls on
  // this handle in the future.
  void* nacl_handle = adapter.get();
  adapters_.insert(std::make_pair(nacl_handle, adapter));
  return nacl_handle;
}

void NaClIPCManager::DestroyChannel(void* handle) {
  scoped_refptr<NaClIPCAdapter> adapter = GetAdapter(handle);
  if (!adapter.get())
    return;
  adapter->CloseChannel();
}

int NaClIPCManager::SendMessage(void* handle,
                                const char* data,
                                int data_length) {
  scoped_refptr<NaClIPCAdapter> adapter = GetAdapter(handle);
  if (!adapter.get())
    return -1;
  return adapter->Send(data, data_length);
}

int NaClIPCManager::ReceiveMessage(void* handle,
                                   char* buffer,
                                   int buffer_length) {
  scoped_refptr<NaClIPCAdapter> adapter = GetAdapter(handle);
  if (!adapter.get())
    return -1;
  return adapter->BlockingReceive(buffer, buffer_length);
}

scoped_refptr<NaClIPCAdapter> NaClIPCManager::GetAdapter(void* handle) {
  base::AutoLock lock(lock_);
  AdapterMap::iterator found = adapters_.find(handle);
  if (found == adapters_.end())
    return scoped_refptr<NaClIPCAdapter>();
  return found->second;
}

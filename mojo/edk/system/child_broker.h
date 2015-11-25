// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHILD_BROKER_H_
#define MOJO_EDK_SYSTEM_CHILD_BROKER_H_

#include "base/memory/singleton.h"
#include "base/synchronization/lock_impl.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {
struct BrokerMessage;

// An implementation of Broker used in (sandboxed) child processes. It talks
// over sync IPCs to the (unsandboxed) parent process (specifically,
// ParentBroker) to convert handles to tokens and vice versa.
class MOJO_SYSTEM_IMPL_EXPORT ChildBroker : public Broker {
 public:
  static ChildBroker* GetInstance();

  // Passes the platform handle that is used to talk to ChildBrokerHost.
  void SetChildBrokerHostHandle(ScopedPlatformHandle handle);

  // Broker implementation:
#if defined(OS_WIN)
  void CreatePlatformChannelPair(ScopedPlatformHandle* server,
                                 ScopedPlatformHandle* client) override;
  void HandleToToken(const PlatformHandle* platform_handles,
                     size_t count,
                     uint64_t* tokens) override;
  void TokenToHandle(const uint64_t* tokens,
                     size_t count,
                     PlatformHandle* handles) override;
#endif

 private:
  friend struct base::DefaultSingletonTraits<ChildBroker>;

  ChildBroker();
  ~ChildBroker() override;

  // Helper method to write the given message and read back the result.
  bool WriteAndReadResponse(BrokerMessage* message,
                            void* response,
                            uint32_t response_size);

  // Guards access to below.
  // We use LockImpl instead of Lock because the latter adds thread checking
  // that we don't want (since we lock in the constructor and unlock on another
  // thread.
  base::internal::LockImpl lock_;
  ScopedPlatformHandle handle_;
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHILD_BROKER_H_

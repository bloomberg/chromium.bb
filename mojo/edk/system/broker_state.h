// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_BROKER_STATE_H_
#define MOJO_EDK_SYSTEM_BROKER_STATE_H_

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

// Common broker state that has to live in a parent process. There is one
// instance of this class in the parent process. This class implements the
// Broker interface for use by code in the parent process as well.
class MOJO_SYSTEM_IMPL_EXPORT BrokerState : NON_EXPORTED_BASE(public Broker) {
 public:
  static BrokerState* GetInstance();

  // Broker implementation.
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

  scoped_refptr<base::TaskRunner> broker_thread() {
    return broker_thread_.task_runner();
  }

 private:
  friend struct base::DefaultSingletonTraits<BrokerState>;

  BrokerState();
  ~BrokerState() override;

  // A separate thread to handle sync IPCs from child processes for exchanging
  // platform handles with tokens. We use a separate thread because latency is
  // very sensitive (since any time a pipe is created or sent, a child process
  // makes a sync call to this class).
  base::Thread broker_thread_;

#if defined(OS_WIN)
  // Used in the parent (unsandboxed) process to hold a mapping between HANDLES
  // and tokens. When a child process wants to send a HANDLE to another process,
  // it exchanges it to a token and then the other process exchanges that token
  // back to a HANDLE.
  base::Lock lock_;  // Guards access to below.
  base::hash_map<uint64_t, HANDLE> token_map_;
#endif
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_BROKER_STATE_H_

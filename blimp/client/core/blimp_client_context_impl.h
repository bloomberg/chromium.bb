// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_BLIMP_CLIENT_CONTEXT_IMPL_H_
#define BLIMP_CLIENT_CORE_BLIMP_CLIENT_CONTEXT_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/supports_user_data.h"
#include "base/threading/thread.h"
#include "blimp/client/core/session/client_network_components.h"
#include "blimp/client/core/session/network_event_observer.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/net/blimp_connection_statistics.h"
#include "blimp/net/thread_pipe_manager.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif  // defined(OS_ANDROID)

namespace blimp {
namespace client {

#if defined(OS_ANDROID)
class BlimpClientContextImplAndroid;
#endif  // defined(OS_ANDROID)

// BlimpClientContextImpl is the implementation of the main context-class for
// the blimp client.
class BlimpClientContextImpl : public BlimpClientContext,
                               public NetworkEventObserver,
                               base::SupportsUserData {
 public:
  // The |io_thread_task_runner| must be the task runner to use for IO
  // operations.
  explicit BlimpClientContextImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);
  ~BlimpClientContextImpl() override;

#if defined(OS_ANDROID)
  BlimpClientContextImplAndroid* GetBlimpClientContextImplAndroid();
#endif  // defined(OS_ANDROID)

// BlimpClientContext implementation.
#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
#endif  // defined(OS_ANDROID)
  void SetDelegate(BlimpClientContextDelegate* delegate) override;
  std::unique_ptr<BlimpContents> CreateBlimpContents() override;

  // NetworkEventObserver implementation.
  void OnConnected() override;
  void OnDisconnected(int result) override;

 private:
  // Provides functionality from the embedder.
  BlimpClientContextDelegate* delegate_;

  // The task runner to use for IO operations.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  // Collects details of network, such as number of commits and bytes
  // transferred over network. Owned by ClientNetworkComponents.
  BlimpConnectionStatistics* blimp_connection_statistics_;

  // Container struct for network components.
  // Must be deleted on the IO thread.
  std::unique_ptr<ClientNetworkComponents> net_components_;

  std::unique_ptr<ThreadPipeManager> thread_pipe_manager_;

  base::WeakPtrFactory<BlimpClientContextImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_BLIMP_CLIENT_CONTEXT_IMPL_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_PROFILING_CLIENT_H_
#define CHROME_COMMON_PROFILING_PROFILING_CLIENT_H_

#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/handle.h"

namespace content {
class ServiceManagerConnection;
}  // namespace content

namespace profiling {

class MemlogSenderPipe;

// The ProfilingClient listens on the interface for a StartProfiling message. On
// receiving the message, it begins profiling the current process.
class ProfilingClient : public mojom::ProfilingClient {
 public:
  ProfilingClient();
  ~ProfilingClient() override;

  // mojom::ProfilingClient overrides:
  void StartProfiling(mojom::ProfilingParamsPtr params) override;
  void FlushMemlogPipe(uint32_t barrier_id) override;

  void OnServiceManagerConnected(content::ServiceManagerConnection* connection);
  void BindToInterface(profiling::mojom::ProfilingClientRequest request);

 private:
  // Ideally, this would be a mojo::Binding that would only keep alive one
  // client request. However, the service that makes the client requests
  // [content_browser] is different from the service that dedupes the client
  // requests [profiling service]. This means that there may be a brief
  // intervals where there are two active bindings, until the profiling service
  // has a chance to figure out which one to keep.
  mojo::BindingSet<mojom::ProfilingClient> bindings_;

  bool started_profiling_;

  std::unique_ptr<MemlogSenderPipe> memlog_sender_pipe_;
};

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_PROFILING_CLIENT_H_

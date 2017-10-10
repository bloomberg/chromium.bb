// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_PROFILING_CLIENT_H_
#define CHROME_COMMON_PROFILING_PROFILING_CLIENT_H_

#include "chrome/common/profiling/profiling_client.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/handle.h"

namespace content {
class ServiceManagerConnection;
}  // namespace content

namespace profiling {

class MemlogSenderPipe;

// The MemlogClient listens on the interface for a StartProfiling message. On
// receiving the message, it begins profiling the current process.
// It is also possible to use the MemlogClient to begin profiling the current
// process without connecting to the service manager interface, if the caller
// has a |sender_pipe| to pass to StartProfiling.
class ProfilingClient : public mojom::ProfilingClient {
 public:
  ProfilingClient();
  ~ProfilingClient() override;

  // mojom::MemlogClient overrides:
  void StartProfiling(mojo::ScopedHandle memlog_sender_pipe) override;
  void FlushMemlogPipe(uint32_t barrier_id) override;

  void OnServiceManagerConnected(content::ServiceManagerConnection* connection);
  void BindToInterface(profiling::mojom::ProfilingClientRequest request);

 private:
  // The most recent client request is bound and kept alive.
  mojo::Binding<mojom::ProfilingClient> binding_;

  std::unique_ptr<MemlogSenderPipe> memlog_sender_pipe_;
};

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_PROFILING_CLIENT_H_

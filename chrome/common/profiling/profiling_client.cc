// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/profiling_client.h"

#include "base/files/platform_file.h"
#include "chrome/common/profiling/memlog_allocator_shim.h"
#include "chrome/common/profiling/memlog_sender_pipe.h"
#include "chrome/common/profiling/memlog_stream.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace profiling {

ProfilingClient::ProfilingClient() : binding_(this) {}

ProfilingClient::~ProfilingClient() {
  StopAllocatorShimDangerous();

  // The allocator shim cannot be synchronously, consistently stopped. We leak
  // the memlog_sender_pipe_, with the idea that very few future messages will
  // be sent to it. This happens at shutdown, so resources will be reclaimed by
  // the OS after the process is terminated.
  memlog_sender_pipe_.release();
}

void ProfilingClient::OnServiceManagerConnected(
    content::ServiceManagerConnection* connection) {
  std::unique_ptr<service_manager::BinderRegistry> registry(
      new service_manager::BinderRegistry);
  registry->AddInterface(base::Bind(
      &profiling::ProfilingClient::BindToInterface, base::Unretained(this)));
  connection->AddConnectionFilter(
      base::MakeUnique<content::SimpleConnectionFilter>(std::move(registry)));
}

void ProfilingClient::BindToInterface(mojom::ProfilingClientRequest request) {
  binding_.Bind(std::move(request));
}

void ProfilingClient::StartProfiling(mojo::ScopedHandle memlog_sender_pipe) {
  base::PlatformFile platform_file;
  CHECK_EQ(MOJO_RESULT_OK, mojo::UnwrapPlatformFile(
                               std::move(memlog_sender_pipe), &platform_file));

  base::ScopedPlatformFile scoped_platform_file(platform_file);
  memlog_sender_pipe_.reset(
      new MemlogSenderPipe(std::move(scoped_platform_file)));

  StreamHeader header;
  header.signature = kStreamSignature;
  memlog_sender_pipe_->Send(&header, sizeof(header));

  InitAllocatorShim(memlog_sender_pipe_.get());
}

void ProfilingClient::FlushMemlogPipe(uint32_t barrier_id) {
  AllocatorShimFlushPipe(barrier_id);
}

}  // namespace profiling

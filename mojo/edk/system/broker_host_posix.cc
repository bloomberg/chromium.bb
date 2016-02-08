// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/broker_host.h"

#include <fcntl.h>
#include <unistd.h>

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/broker_messages.h"

namespace mojo {
namespace edk {

namespace {
// To prevent abuse, limit the maximum size of shared memory buffers.
// TODO(amistry): Re-consider this limit, or do something smarter.
const size_t kMaxSharedBufferSize = 16 * 1024 * 1024;
}

BrokerHost::BrokerHost(ScopedPlatformHandle platform_handle) {
  CHECK(platform_handle.is_valid());

  base::MessageLoop::current()->AddDestructionObserver(this);

  channel_ = Channel::Create(this, std::move(platform_handle),
                             base::MessageLoop::current()->task_runner());
  channel_->Start();
}

BrokerHost::~BrokerHost() {
  // We're always destroyed on the creation thread, which is the IO thread.
  base::MessageLoop::current()->RemoveDestructionObserver(this);

  if (channel_)
    channel_->ShutDown();
}

void BrokerHost::SendChannel(ScopedPlatformHandle handle) {
  CHECK(handle.is_valid());
  CHECK(channel_);

  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 1, nullptr);
  ScopedPlatformHandleVectorPtr handles;
  handles.reset(new PlatformHandleVector(1));
  handles->at(0) = handle.release();
  message->SetHandles(std::move(handles));

  channel_->Write(std::move(message));
}

void BrokerHost::OnBufferRequest(size_t num_bytes) {
  scoped_refptr<PlatformSharedBuffer> buffer;
  if (num_bytes <= kMaxSharedBufferSize) {
    buffer = internal::g_platform_support->CreateSharedBuffer(num_bytes);
  } else {
    LOG(ERROR) << "Shared buffer request too large: " << num_bytes;
  }

  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_RESPONSE, buffer ? 1 : 0, nullptr);
  if (buffer) {
    ScopedPlatformHandleVectorPtr handles;
    handles.reset(new PlatformHandleVector(1));
    handles->at(0) = buffer->PassPlatformHandle().release();
    message->SetHandles(std::move(handles));
  }

  channel_->Write(std::move(message));
}

void BrokerHost::OnChannelMessage(const void* payload,
                                  size_t payload_size,
                                  ScopedPlatformHandleVectorPtr handles) {
  const BrokerMessageHeader* header =
      static_cast<const BrokerMessageHeader*>(payload);
  switch (header->type) {
    case BrokerMessageType::BUFFER_REQUEST: {
      const BufferRequestData* request =
          reinterpret_cast<const BufferRequestData*>(header + 1);
      OnBufferRequest(request->size);
      break;
    }
    default:
      LOG(ERROR) << "Unexpected broker message type: " << header->type;
      break;
  }
}

void BrokerHost::OnChannelError() {
  if (channel_) {
    channel_->ShutDown();
    channel_ = nullptr;
  }

  delete this;
}

void BrokerHost::WillDestroyCurrentMessageLoop() {
  delete this;
}

}  // namespace edk
}  // namespace mojo

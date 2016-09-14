// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "mojo/edk/embedder/platform_handle.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/broker_messages.h"
#include "mojo/edk/system/channel.h"

namespace mojo {
namespace edk {

namespace {

// 256 bytes should be enough for anyone!
const size_t kMaxBrokerMessageSize = 256;

bool WaitForBrokerMessage(PlatformHandle platform_handle,
                          BrokerMessageType expected_type,
                          size_t expected_num_handles,
                          ScopedPlatformHandle* out_handles) {
  char buffer[kMaxBrokerMessageSize];
  DWORD bytes_read = 0;
  BOOL result = ::ReadFile(platform_handle.handle, buffer,
                           kMaxBrokerMessageSize, &bytes_read, nullptr);
  if (!result) {
    PLOG(ERROR) << "Error reading broker pipe";
    return false;
  }

  Channel::MessagePtr message =
      Channel::Message::Deserialize(buffer, static_cast<size_t>(bytes_read));
  if (!message || message->payload_size() < sizeof(BrokerMessageHeader)) {
    LOG(ERROR) << "Invalid broker message";
    return false;
  }

  if (message->num_handles() != expected_num_handles) {
    LOG(ERROR) << "Received unexpected number of handles in broker message";
    return false;
  }

  const BrokerMessageHeader* header =
      reinterpret_cast<const BrokerMessageHeader*>(message->payload());
  if (header->type != expected_type) {
    LOG(ERROR) << "Unknown broker message type";
    return false;
  }

  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  DCHECK(handles);
  DCHECK_EQ(handles->size(), expected_num_handles);
  DCHECK(out_handles);

  for (size_t i = 0; i < expected_num_handles; ++i)
    out_handles[i] = ScopedPlatformHandle(handles->at(i));
  handles->clear();
  return true;
}

}  // namespace

Broker::Broker(ScopedPlatformHandle handle) : sync_channel_(std::move(handle)) {
  CHECK(sync_channel_.is_valid());
  bool result = WaitForBrokerMessage(
      sync_channel_.get(), BrokerMessageType::INIT, 1, &parent_channel_);
  DCHECK(result);
}

Broker::~Broker() {}

ScopedPlatformHandle Broker::GetParentPlatformHandle() {
  return std::move(parent_channel_);
}

scoped_refptr<PlatformSharedBuffer> Broker::GetSharedBuffer(size_t num_bytes) {
  base::AutoLock lock(lock_);
  BufferRequestData* buffer_request;
  Channel::MessagePtr out_message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_REQUEST, 0, &buffer_request);
  buffer_request->size = base::checked_cast<uint32_t>(num_bytes);
  DWORD bytes_written = 0;
  BOOL result = ::WriteFile(sync_channel_.get().handle, out_message->data(),
                            static_cast<DWORD>(out_message->data_num_bytes()),
                            &bytes_written, nullptr);
  if (!result ||
      static_cast<size_t>(bytes_written) != out_message->data_num_bytes()) {
    LOG(ERROR) << "Error sending sync broker message";
    return nullptr;
  }

  ScopedPlatformHandle handles[2];
  if (WaitForBrokerMessage(sync_channel_.get(),
                           BrokerMessageType::BUFFER_RESPONSE, 2,
                           &handles[0])) {
    return PlatformSharedBuffer::CreateFromPlatformHandlePair(
        num_bytes, std::move(handles[0]), std::move(handles[1]));
  }

  return nullptr;
}

}  // namespace edk
}  // namespace mojo

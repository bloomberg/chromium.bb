// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/broker_host.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/edk/embedder/named_platform_channel_pair.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/broker_messages.h"

namespace mojo {
namespace edk {

BrokerHost::BrokerHost(base::ProcessHandle client_process,
                       ScopedPlatformHandle platform_handle,
                       const ProcessErrorCallback& process_error_callback)
    : process_error_callback_(process_error_callback)
#if defined(OS_WIN)
      ,
      client_process_(client_process)
#endif
{
  CHECK(platform_handle.is_valid());

  base::MessageLoop::current()->AddDestructionObserver(this);

  channel_ = Channel::Create(
      this,
      ConnectionParams(TransportProtocol::kLegacy, std::move(platform_handle)),
      base::ThreadTaskRunnerHandle::Get());
  channel_->Start();
}

BrokerHost::~BrokerHost() {
  // We're always destroyed on the creation thread, which is the IO thread.
  base::MessageLoop::current()->RemoveDestructionObserver(this);

  if (channel_)
    channel_->ShutDown();
}

bool BrokerHost::PrepareHandlesForClient(
    std::vector<ScopedPlatformHandle>* handles) {
#if defined(OS_WIN)
  if (!Channel::Message::RewriteHandles(base::GetCurrentProcessHandle(),
                                        client_process_, handles)) {
    // NOTE: We only log an error here. We do not signal a logical error or
    // prevent any message from being sent. The client should handle unexpected
    // invalid handles appropriately.
    DLOG(ERROR) << "Failed to rewrite one or more handles to broker client.";
    return false;
  }
#endif
  return true;
}

bool BrokerHost::SendChannel(ScopedPlatformHandle handle) {
  CHECK(handle.is_valid());
  CHECK(channel_);

#if defined(OS_WIN)
  InitData* data;
  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 1, 0, &data);
  data->pipe_name_length = 0;
#else
  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 1, nullptr);
#endif
  std::vector<ScopedPlatformHandle> handles(1);
  handles[0] = std::move(handle);

  // This may legitimately fail on Windows if the client process is in another
  // session, e.g., is an elevated process.
  if (!PrepareHandlesForClient(&handles))
    return false;

  message->SetHandles(std::move(handles));
  channel_->Write(std::move(message));
  return true;
}

#if defined(OS_WIN)

void BrokerHost::SendNamedChannel(const base::StringPiece16& pipe_name) {
  InitData* data;
  base::char16* name_data;
  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::INIT, 0, sizeof(*name_data) * pipe_name.length(),
      &data, reinterpret_cast<void**>(&name_data));
  data->pipe_name_length = static_cast<uint32_t>(pipe_name.length());
  std::copy(pipe_name.begin(), pipe_name.end(), name_data);
  channel_->Write(std::move(message));
}

#endif  // defined(OS_WIN)

void BrokerHost::OnBufferRequest(uint32_t num_bytes) {
  scoped_refptr<PlatformSharedBuffer> read_only_buffer;
  scoped_refptr<PlatformSharedBuffer> buffer =
      PlatformSharedBuffer::Create(num_bytes);
  if (buffer)
    read_only_buffer = buffer->CreateReadOnlyDuplicate();
  if (!read_only_buffer)
    buffer = nullptr;

  BufferResponseData* response;
  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_RESPONSE, buffer ? 2 : 0, 0, &response);
  if (buffer) {
    base::UnguessableToken guid = buffer->GetGUID();
    response->guid_high = guid.GetHighForSerialization();
    response->guid_low = guid.GetLowForSerialization();
    std::vector<ScopedPlatformHandle> handles(2);
    handles[0] = buffer->PassPlatformHandle();
    handles[1] = read_only_buffer->PassPlatformHandle();
    PrepareHandlesForClient(&handles);
    message->SetHandles(std::move(handles));
  }

  channel_->Write(std::move(message));
}

void BrokerHost::OnChannelMessage(const void* payload,
                                  size_t payload_size,
                                  std::vector<ScopedPlatformHandle> handles) {
  if (payload_size < sizeof(BrokerMessageHeader))
    return;

  const BrokerMessageHeader* header =
      static_cast<const BrokerMessageHeader*>(payload);
  switch (header->type) {
    case BrokerMessageType::BUFFER_REQUEST:
      if (payload_size ==
          sizeof(BrokerMessageHeader) + sizeof(BufferRequestData)) {
        const BufferRequestData* request =
            reinterpret_cast<const BufferRequestData*>(header + 1);
        OnBufferRequest(request->size);
      }
      break;

    default:
      DLOG(ERROR) << "Unexpected broker message type: " << header->type;
      break;
  }
}

void BrokerHost::OnChannelError(Channel::Error error) {
  if (process_error_callback_ &&
      error == Channel::Error::kReceivedMalformedData) {
    process_error_callback_.Run("Broker host received malformed message");
  }

  delete this;
}

void BrokerHost::WillDestroyCurrentMessageLoop() {
  delete this;
}

}  // namespace edk
}  // namespace mojo

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/child_broker.h"

#include "base/logging.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/broker_messages.h"

namespace mojo {
namespace edk {

ChildBroker* ChildBroker::GetInstance() {
  return base::Singleton<
      ChildBroker, base::LeakySingletonTraits<ChildBroker>>::get();
}

void ChildBroker::SetChildBrokerHostHandle(ScopedPlatformHandle handle)  {
  handle_ = handle.Pass();
  lock_.Unlock();
}

#if defined(OS_WIN)
void ChildBroker::CreatePlatformChannelPair(
    ScopedPlatformHandle* server, ScopedPlatformHandle* client) {
  BrokerMessage message;
  message.size = kBrokerMessageHeaderSize;
  message.id = CREATE_PLATFORM_CHANNEL_PAIR;

  uint32_t response_size = 2 * sizeof(HANDLE);
  HANDLE handles[2];
  if (WriteAndReadResponse(&message, handles, response_size)) {
    server->reset(PlatformHandle(handles[0]));
    client->reset(PlatformHandle(handles[1]));
  }
}

void ChildBroker::HandleToToken(const PlatformHandle* platform_handles,
                                size_t count,
                                uint64_t* tokens) {
  uint32_t size = kBrokerMessageHeaderSize +
                  static_cast<int>(count) * sizeof(HANDLE);
  std::vector<char> message_buffer(size);
  BrokerMessage* message = reinterpret_cast<BrokerMessage*>(&message_buffer[0]);
  message->size = size;
  message->id = HANDLE_TO_TOKEN;
  for (size_t i = 0; i < count; ++i)
    message->handles[i] = platform_handles[i].handle;

  uint32_t response_size = static_cast<int>(count) * sizeof(uint64_t);
  WriteAndReadResponse(message, tokens, response_size);
}

void ChildBroker::TokenToHandle(const uint64_t* tokens,
                                size_t count,
                                PlatformHandle* handles) {
  uint32_t size = kBrokerMessageHeaderSize +
                  static_cast<int>(count) * sizeof(uint64_t);
  std::vector<char> message_buffer(size);
  BrokerMessage* message =
      reinterpret_cast<BrokerMessage*>(&message_buffer[0]);
  message->size = size;
  message->id = TOKEN_TO_HANDLE;
  memcpy(&message->tokens[0], tokens, count * sizeof(uint64_t));

  std::vector<HANDLE> handles_temp(count);
  uint32_t response_size =
      static_cast<uint32_t>(handles_temp.size()) * sizeof(HANDLE);
  if (WriteAndReadResponse(message, &handles_temp[0], response_size)) {
    for (uint32_t i = 0; i < count; ++i)
      handles[i].handle = handles_temp[i];
  }
}
#endif

ChildBroker::ChildBroker() {
  DCHECK(!internal::g_broker);
  internal::g_broker = this;
  // Block any threads from calling this until we have a pipe to the parent.
  lock_.Lock();
}

ChildBroker::~ChildBroker() {
}

bool ChildBroker::WriteAndReadResponse(BrokerMessage* message,
                                       void* response,
                                       uint32_t response_size) {
  lock_.Lock();
  CHECK(handle_.is_valid());

  bool result = true;
#if defined(OS_WIN)
  DWORD bytes_written = 0;
  // This will always write in one chunk per
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365150.aspx.
  BOOL rv = WriteFile(handle_.get().handle, message, message->size,
                      &bytes_written, NULL);
  if (!rv || bytes_written != message->size) {
    LOG(ERROR) << "Child token serializer couldn't write message.";
    result = false;
  } else {
    while (response_size) {
      DWORD bytes_read = 0;
      rv = ReadFile(handle_.get().handle, response, response_size, &bytes_read,
                    NULL);
      if (!rv) {
        LOG(ERROR) << "Child token serializer couldn't read result.";
        result = false;
        break;
      }
      response_size -= bytes_read;
      response = static_cast<char*>(response) + bytes_read;
    }
  }
#endif

  lock_.Unlock();

  return result;
}

}  // namespace edk
}  // namespace mojo

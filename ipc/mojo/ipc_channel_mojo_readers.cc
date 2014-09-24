// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo_readers.h"

#include "ipc/mojo/ipc_channel_mojo.h"
#include "mojo/embedder/embedder.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include "ipc/file_descriptor_set_posix.h"
#endif

namespace IPC {
namespace internal {

namespace {

// TODO(morrita): This should be built using higher-level Mojo construct
// for clarity and extensibility.
class HelloMessage {
 public:
  static Pickle CreateRequest(int32 pid) {
    Pickle request;
    request.WriteString(kHelloRequestMagic);
    request.WriteInt(pid);
    return request;
  }

  static bool ReadRequest(Pickle& pickle, int32* pid) {
    PickleIterator iter(pickle);
    std::string hello;
    if (!iter.ReadString(&hello)) {
      DLOG(WARNING) << "Failed to Read magic string.";
      return false;
    }

    if (hello != kHelloRequestMagic) {
      DLOG(WARNING) << "Magic mismatch:" << hello;
      return false;
    }

    int read_pid;
    if (!iter.ReadInt(&read_pid)) {
      DLOG(WARNING) << "Failed to Read PID.";
      return false;
    }

    *pid = read_pid;
    return true;
  }

  static Pickle CreateResponse(int32 pid) {
    Pickle request;
    request.WriteString(kHelloResponseMagic);
    request.WriteInt(pid);
    return request;
  }

  static bool ReadResponse(Pickle& pickle, int32* pid) {
    PickleIterator iter(pickle);
    std::string hello;
    if (!iter.ReadString(&hello)) {
      DLOG(WARNING) << "Failed to read magic string.";
      return false;
    }

    if (hello != kHelloResponseMagic) {
      DLOG(WARNING) << "Magic mismatch:" << hello;
      return false;
    }

    int read_pid;
    if (!iter.ReadInt(&read_pid)) {
      DLOG(WARNING) << "Failed to read PID.";
      return false;
    }

    *pid = read_pid;
    return true;
  }

 private:
  static const char* kHelloRequestMagic;
  static const char* kHelloResponseMagic;
};

const char* HelloMessage::kHelloRequestMagic = "MREQ";
const char* HelloMessage::kHelloResponseMagic = "MRES";

}  // namespace

//------------------------------------------------------------------------------

MessageReader::MessageReader(mojo::ScopedMessagePipeHandle pipe,
                             ChannelMojo* owner)
    : internal::MessagePipeReader(pipe.Pass()), owner_(owner) {
}

void MessageReader::OnMessageReceived() {
  Message message(data_buffer().empty() ? "" : &data_buffer()[0],
                  static_cast<uint32>(data_buffer().size()));

  std::vector<MojoHandle> handle_buffer;
  TakeHandleBuffer(&handle_buffer);
#if defined(OS_POSIX) && !defined(OS_NACL)
  MojoResult write_result =
      ChannelMojo::WriteToFileDescriptorSet(handle_buffer, &message);
  if (write_result != MOJO_RESULT_OK) {
    CloseWithError(write_result);
    return;
  }
#else
  DCHECK(handle_buffer.empty());
#endif

  message.TraceMessageEnd();
  owner_->OnMessageReceived(message);
}

void MessageReader::OnPipeClosed() {
  if (!owner_)
    return;
  owner_->OnPipeClosed(this);
  owner_ = NULL;
}

void MessageReader::OnPipeError(MojoResult error) {
  if (!owner_)
    return;
  owner_->OnPipeError(this);
}

bool MessageReader::Send(scoped_ptr<Message> message) {
  DCHECK(IsValid());

  message->TraceMessageBegin();
  std::vector<MojoHandle> handles;
#if defined(OS_POSIX) && !defined(OS_NACL)
  MojoResult read_result =
      ChannelMojo::ReadFromFileDescriptorSet(message.get(), &handles);
  if (read_result != MOJO_RESULT_OK) {
    std::for_each(handles.begin(), handles.end(), &MojoClose);
    CloseWithError(read_result);
    return false;
  }
#endif
  MojoResult write_result =
      MojoWriteMessage(handle(),
                       message->data(),
                       static_cast<uint32>(message->size()),
                       handles.empty() ? NULL : &handles[0],
                       static_cast<uint32>(handles.size()),
                       MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (MOJO_RESULT_OK != write_result) {
    std::for_each(handles.begin(), handles.end(), &MojoClose);
    CloseWithError(write_result);
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------

ControlReader::ControlReader(mojo::ScopedMessagePipeHandle pipe,
                             ChannelMojo* owner)
    : internal::MessagePipeReader(pipe.Pass()), owner_(owner) {
}

void ControlReader::OnPipeClosed() {
  if (!owner_)
    return;
  owner_->OnPipeClosed(this);
  owner_ = NULL;
}

void ControlReader::OnPipeError(MojoResult error) {
  if (!owner_)
    return;
  owner_->OnPipeError(this);
}

bool ControlReader::Connect() {
  return true;
}

//------------------------------------------------------------------------------

ServerControlReader::ServerControlReader(mojo::ScopedMessagePipeHandle pipe,
                                         ChannelMojo* owner)
    : ControlReader(pipe.Pass(), owner) {
}

ServerControlReader::~ServerControlReader() {
}

bool ServerControlReader::Connect() {
  MojoResult result = SendHelloRequest();
  if (result != MOJO_RESULT_OK) {
    CloseWithError(result);
    return false;
  }

  return true;
}

MojoResult ServerControlReader::SendHelloRequest() {
  DCHECK(IsValid());
  DCHECK(!message_pipe_.is_valid());

  mojo::ScopedMessagePipeHandle self;
  mojo::ScopedMessagePipeHandle peer;
  MojoResult create_result =
      mojo::CreateMessagePipe(NULL, &message_pipe_, &peer);
  if (MOJO_RESULT_OK != create_result) {
    DLOG(WARNING) << "mojo::CreateMessagePipe failed: " << create_result;
    return create_result;
  }

  MojoHandle peer_to_send = peer.get().value();
  Pickle request = HelloMessage::CreateRequest(owner_->GetSelfPID());
  MojoResult write_result =
      MojoWriteMessage(handle(),
                       request.data(),
                       static_cast<uint32>(request.size()),
                       &peer_to_send,
                       1,
                       MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (MOJO_RESULT_OK != write_result) {
    DLOG(WARNING) << "Writing Hello request failed: " << create_result;
    return write_result;
  }

  // |peer| is sent and no longer owned by |this|.
  (void)peer.release();
  return MOJO_RESULT_OK;
}

MojoResult ServerControlReader::RespondHelloResponse() {
  Pickle request(data_buffer().empty() ? "" : &data_buffer()[0],
                 static_cast<uint32>(data_buffer().size()));

  int32 read_pid = 0;
  if (!HelloMessage::ReadResponse(request, &read_pid)) {
    DLOG(ERROR) << "Failed to parse Hello response.";
    return MOJO_RESULT_UNKNOWN;
  }

  base::ProcessId pid = static_cast<base::ProcessId>(read_pid);
  owner_->set_peer_pid(pid);
  owner_->OnConnected(message_pipe_.Pass());
  return MOJO_RESULT_OK;
}

void ServerControlReader::OnMessageReceived() {
  MojoResult result = RespondHelloResponse();
  if (result != MOJO_RESULT_OK)
    CloseWithError(result);
}

//------------------------------------------------------------------------------

ClientControlReader::ClientControlReader(mojo::ScopedMessagePipeHandle pipe,
                                         ChannelMojo* owner)
    : ControlReader(pipe.Pass(), owner) {
}

MojoResult ClientControlReader::RespondHelloRequest(
    MojoHandle message_channel) {
  DCHECK(IsValid());

  mojo::ScopedMessagePipeHandle received_pipe(
      (mojo::MessagePipeHandle(message_channel)));

  int32 read_request = 0;
  Pickle request(data_buffer().empty() ? "" : &data_buffer()[0],
                 static_cast<uint32>(data_buffer().size()));
  if (!HelloMessage::ReadRequest(request, &read_request)) {
    DLOG(ERROR) << "Hello request has wrong magic.";
    return MOJO_RESULT_UNKNOWN;
  }

  base::ProcessId pid = read_request;
  Pickle response = HelloMessage::CreateResponse(owner_->GetSelfPID());
  MojoResult write_result =
      MojoWriteMessage(handle(),
                       response.data(),
                       static_cast<uint32>(response.size()),
                       NULL,
                       0,
                       MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (MOJO_RESULT_OK != write_result) {
    DLOG(ERROR) << "Writing Hello response failed: " << write_result;
    return write_result;
  }

  owner_->set_peer_pid(pid);
  owner_->OnConnected(received_pipe.Pass());
  return MOJO_RESULT_OK;
}

void ClientControlReader::OnMessageReceived() {
  std::vector<MojoHandle> handle_buffer;
  TakeHandleBuffer(&handle_buffer);
  if (handle_buffer.size() != 1) {
    DLOG(ERROR) << "Hello request doesn't contains required handle: "
                << handle_buffer.size();
    CloseWithError(MOJO_RESULT_UNKNOWN);
    return;
  }

  MojoResult result = RespondHelloRequest(handle_buffer[0]);
  if (result != MOJO_RESULT_OK) {
    DLOG(ERROR) << "Failed to respond Hello request. Closing: " << result;
    CloseWithError(result);
  }
}

}  // namespace internal
}  // namespace IPC

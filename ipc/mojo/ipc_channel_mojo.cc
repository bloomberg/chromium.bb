// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "ipc/ipc_listener.h"
#include "mojo/embedder/embedder.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include "ipc/file_descriptor_set_posix.h"
#endif

namespace IPC {

namespace {

// IPC::Listener for bootstrap channels.
// It should never receive any message.
class NullListener : public Listener {
 public:
  virtual bool OnMessageReceived(const Message&) OVERRIDE {
    NOTREACHED();
    return false;
  }

  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
    NOTREACHED();
  }

  virtual void OnChannelError() OVERRIDE {
    NOTREACHED();
  }

  virtual void OnBadMessageReceived(const Message& message) OVERRIDE {
    NOTREACHED();
  }
};

base::LazyInstance<NullListener> g_null_listener = LAZY_INSTANCE_INITIALIZER;

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(
      ChannelHandle channel_handle,
      Channel::Mode mode,
      scoped_refptr<base::TaskRunner> io_thread_task_runner)
      : channel_handle_(channel_handle),
        mode_(mode),
        io_thread_task_runner_(io_thread_task_runner) {
  }

  virtual std::string GetName() const OVERRIDE {
    return channel_handle_.name;
  }

  virtual scoped_ptr<Channel> BuildChannel(Listener* listener) OVERRIDE {
    return ChannelMojo::Create(
        channel_handle_,
        mode_,
        listener,
        io_thread_task_runner_).PassAs<Channel>();
  }

 private:
  ChannelHandle channel_handle_;
  Channel::Mode mode_;
  scoped_refptr<base::TaskRunner> io_thread_task_runner_;
};

mojo::embedder::PlatformHandle ToPlatformHandle(
    const ChannelHandle& handle) {
#if defined(OS_POSIX) && !defined(OS_NACL)
  return mojo::embedder::PlatformHandle(handle.socket.fd);
#elif defined(OS_WIN)
  return mojo::embedder::PlatformHandle(handle.pipe.handle);
#else
#error "Unsupported Platform!"
#endif
}

//------------------------------------------------------------------------------

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

} // namespace

//------------------------------------------------------------------------------

// A MessagePipeReader implemenation for IPC::Message communication.
class ChannelMojo::MessageReader : public internal::MessagePipeReader {
 public:
  MessageReader(mojo::ScopedMessagePipeHandle pipe, ChannelMojo* owner)
      : internal::MessagePipeReader(pipe.Pass()),
        owner_(owner) {}

  bool Send(scoped_ptr<Message> message);
  virtual void OnMessageReceived() OVERRIDE;
  virtual void OnPipeClosed() OVERRIDE;
  virtual void OnPipeError(MojoResult error) OVERRIDE;

 private:
  ChannelMojo* owner_;
};

void ChannelMojo::MessageReader::OnMessageReceived() {
  Message message(data_buffer().empty() ? "" : &data_buffer()[0],
                  static_cast<uint32>(data_buffer().size()));

  std::vector<MojoHandle> handle_buffer;
  TakeHandleBuffer(&handle_buffer);
#if defined(OS_POSIX) && !defined(OS_NACL)
  for (size_t i = 0; i < handle_buffer.size(); ++i) {
    mojo::embedder::ScopedPlatformHandle platform_handle;
    MojoResult unwrap_result = mojo::embedder::PassWrappedPlatformHandle(
        handle_buffer[i], &platform_handle);
    if (unwrap_result != MOJO_RESULT_OK) {
      DLOG(WARNING) << "Pipe failed to covert handles. Closing: "
                    << unwrap_result;
      CloseWithError(unwrap_result);
      return;
    }

    bool ok = message.file_descriptor_set()->Add(platform_handle.release().fd);
    DCHECK(ok);
  }
#else
  DCHECK(handle_buffer.empty());
#endif

  message.TraceMessageEnd();
  owner_->OnMessageReceived(message);
}

void ChannelMojo::MessageReader::OnPipeClosed() {
  if (!owner_)
    return;
  owner_->OnPipeClosed(this);
  owner_ = NULL;
}

void ChannelMojo::MessageReader::OnPipeError(MojoResult error) {
  if (!owner_)
    return;
  owner_->OnPipeError(this);
}

bool ChannelMojo::MessageReader::Send(scoped_ptr<Message> message) {
  DCHECK(IsValid());

  message->TraceMessageBegin();
  std::vector<MojoHandle> handles;
#if defined(OS_POSIX) && !defined(OS_NACL)
  if (message->HasFileDescriptors()) {
    FileDescriptorSet* fdset = message->file_descriptor_set();
    for (size_t i = 0; i < fdset->size(); ++i) {
      MojoHandle wrapped_handle;
      MojoResult wrap_result = CreatePlatformHandleWrapper(
          mojo::embedder::ScopedPlatformHandle(
              mojo::embedder::PlatformHandle(
                  fdset->GetDescriptorAt(i))),
          &wrapped_handle);
      if (MOJO_RESULT_OK != wrap_result) {
        DLOG(WARNING) << "Pipe failed to wrap handles. Closing: "
                      << wrap_result;
        CloseWithError(wrap_result);
        return false;
      }

      handles.push_back(wrapped_handle);
    }
  }
#endif
  MojoResult write_result = MojoWriteMessage(
      handle(),
      message->data(), static_cast<uint32>(message->size()),
      handles.empty() ? NULL : &handles[0],
      static_cast<uint32>(handles.size()),
      MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (MOJO_RESULT_OK != write_result) {
    CloseWithError(write_result);
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------

// MessagePipeReader implemenation for control messages.
// Actual message handling is implemented by sublcasses.
class ChannelMojo::ControlReader : public internal::MessagePipeReader {
 public:
  ControlReader(mojo::ScopedMessagePipeHandle pipe, ChannelMojo* owner)
      : internal::MessagePipeReader(pipe.Pass()),
        owner_(owner) {}

  virtual bool Connect() { return true; }
  virtual void OnPipeClosed() OVERRIDE;
  virtual void OnPipeError(MojoResult error) OVERRIDE;

 protected:
  ChannelMojo* owner_;
};

void ChannelMojo::ControlReader::OnPipeClosed() {
  if (!owner_)
    return;
  owner_->OnPipeClosed(this);
  owner_ = NULL;
}

void ChannelMojo::ControlReader::OnPipeError(MojoResult error) {
  if (!owner_)
    return;
  owner_->OnPipeError(this);
}

//------------------------------------------------------------------------------

// ControlReader for server-side ChannelMojo.
class ChannelMojo::ServerControlReader : public ChannelMojo::ControlReader {
 public:
  ServerControlReader(mojo::ScopedMessagePipeHandle pipe, ChannelMojo* owner)
      : ControlReader(pipe.Pass(), owner) { }

  virtual bool Connect() OVERRIDE;
  virtual void OnMessageReceived() OVERRIDE;

 private:
  MojoResult SendHelloRequest();
  MojoResult RespondHelloResponse();

  mojo::ScopedMessagePipeHandle message_pipe_;
};

bool ChannelMojo::ServerControlReader::Connect() {
  MojoResult result = SendHelloRequest();
  if (result != MOJO_RESULT_OK) {
    CloseWithError(result);
    return false;
  }

  return true;
}

MojoResult ChannelMojo::ServerControlReader::SendHelloRequest() {
  DCHECK(IsValid());
  DCHECK(!message_pipe_.is_valid());

  mojo::ScopedMessagePipeHandle self;
  mojo::ScopedMessagePipeHandle peer;
  MojoResult create_result = mojo::CreateMessagePipe(
      NULL, &message_pipe_, &peer);
  if (MOJO_RESULT_OK != create_result) {
    DLOG(WARNING) << "mojo::CreateMessagePipe failed: " << create_result;
    return create_result;
  }

  MojoHandle peer_to_send = peer.get().value();
  Pickle request = HelloMessage::CreateRequest(owner_->GetSelfPID());
  MojoResult write_result = MojoWriteMessage(
      handle(),
      request.data(), static_cast<uint32>(request.size()),
      &peer_to_send, 1,
      MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (MOJO_RESULT_OK != write_result) {
    DLOG(WARNING) << "Writing Hello request failed: " << create_result;
    return write_result;
  }

  // |peer| is sent and no longer owned by |this|.
  (void)peer.release();
  return MOJO_RESULT_OK;
}

MojoResult ChannelMojo::ServerControlReader::RespondHelloResponse() {
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

void ChannelMojo::ServerControlReader::OnMessageReceived() {
  MojoResult result = RespondHelloResponse();
  if (result != MOJO_RESULT_OK)
    CloseWithError(result);
}

//------------------------------------------------------------------------------

// ControlReader for client-side ChannelMojo.
class ChannelMojo::ClientControlReader : public ChannelMojo::ControlReader {
 public:
  ClientControlReader(mojo::ScopedMessagePipeHandle pipe, ChannelMojo* owner)
      : ControlReader(pipe.Pass(), owner) {}

  virtual void OnMessageReceived() OVERRIDE;

 private:
  MojoResult RespondHelloRequest(MojoHandle message_channel);
};

MojoResult ChannelMojo::ClientControlReader::RespondHelloRequest(
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
  MojoResult write_result = MojoWriteMessage(
      handle(),
      response.data(), static_cast<uint32>(response.size()),
      NULL, 0,
      MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (MOJO_RESULT_OK != write_result) {
    DLOG(ERROR) << "Writing Hello response failed: " << write_result;
    return write_result;
  }

  owner_->set_peer_pid(pid);
  owner_->OnConnected(received_pipe.Pass());
  return MOJO_RESULT_OK;
}

void ChannelMojo::ClientControlReader::OnMessageReceived() {
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
    DLOG(ERROR) << "Failed to respond Hello request. Closing: "
                << result;
    CloseWithError(result);
  }
}

//------------------------------------------------------------------------------

void ChannelMojo::ChannelInfoDeleter::operator()(
    mojo::embedder::ChannelInfo* ptr) const {
  mojo::embedder::DestroyChannelOnIOThread(ptr);
}

//------------------------------------------------------------------------------

// static
scoped_ptr<ChannelMojo> ChannelMojo::Create(
    scoped_ptr<Channel> bootstrap, Mode mode, Listener* listener,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  return make_scoped_ptr(new ChannelMojo(
      bootstrap.Pass(), mode, listener, io_thread_task_runner));
}

// static
scoped_ptr<ChannelMojo> ChannelMojo::Create(
    const ChannelHandle &channel_handle, Mode mode, Listener* listener,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  return Create(
      Channel::Create(channel_handle, mode, g_null_listener.Pointer()),
      mode, listener, io_thread_task_runner);
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateFactory(
    const ChannelHandle &channel_handle, Mode mode,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  return make_scoped_ptr(
      new MojoChannelFactory(
          channel_handle, mode,
          io_thread_task_runner)).PassAs<ChannelFactory>();
}

ChannelMojo::ChannelMojo(
    scoped_ptr<Channel> bootstrap, Mode mode, Listener* listener,
    scoped_refptr<base::TaskRunner> io_thread_task_runner)
    : weak_factory_(this),
      bootstrap_(bootstrap.Pass()),
      mode_(mode), listener_(listener),
      peer_pid_(base::kNullProcessId) {
  DCHECK(mode_ == MODE_SERVER || mode_ == MODE_CLIENT);
  mojo::ScopedMessagePipeHandle control_pipe
      = mojo::embedder::CreateChannel(
          mojo::embedder::ScopedPlatformHandle(
              ToPlatformHandle(bootstrap_->TakePipeHandle())),
          io_thread_task_runner,
          base::Bind(&ChannelMojo::DidCreateChannel, base::Unretained(this)),
          io_thread_task_runner);

  // MessagePipeReader, that is crated in InitOnIOThread(), should live only in
  // IO thread, but IPC::Channel can be instantiated outside of it.
  // So we move the creation to the appropriate thread.
  if (base::MessageLoopProxy::current() == io_thread_task_runner) {
    InitOnIOThread(control_pipe.Pass());
  } else {
    io_thread_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ChannelMojo::InitOnIOThread,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(control_pipe.Pass())));
  }
}

ChannelMojo::~ChannelMojo() {
  Close();
}

void ChannelMojo::InitOnIOThread(mojo::ScopedMessagePipeHandle control_pipe) {
  control_reader_ = CreateControlReader(control_pipe.Pass());
}

scoped_ptr<ChannelMojo::ControlReader> ChannelMojo::CreateControlReader(
    mojo::ScopedMessagePipeHandle pipe) {
  if (MODE_SERVER == mode_) {
    return make_scoped_ptr(
        new ServerControlReader(pipe.Pass(), this)).PassAs<ControlReader>();
  }

  DCHECK(mode_ == MODE_CLIENT);
  return make_scoped_ptr(
      new ClientControlReader(pipe.Pass(), this)).PassAs<ControlReader>();
}

bool ChannelMojo::Connect() {
  DCHECK(!message_reader_);
  return control_reader_->Connect();
}

void ChannelMojo::Close() {
  control_reader_.reset();
  message_reader_.reset();
  channel_info_.reset();
}

void ChannelMojo::OnConnected(mojo::ScopedMessagePipeHandle pipe) {
  message_reader_ = make_scoped_ptr(new MessageReader(pipe.Pass(), this));

  for (size_t i = 0; i < pending_messages_.size(); ++i) {
    message_reader_->Send(make_scoped_ptr(pending_messages_[i]));
    pending_messages_[i] = NULL;
  }

  pending_messages_.clear();

  listener_->OnChannelConnected(GetPeerPID());
}

void ChannelMojo::OnPipeClosed(internal::MessagePipeReader* reader) {
  Close();
}

void ChannelMojo::OnPipeError(internal::MessagePipeReader* reader) {
  listener_->OnChannelError();
}


bool ChannelMojo::Send(Message* message) {
  if (!message_reader_) {
    pending_messages_.push_back(message);
    return true;
  }

  return message_reader_->Send(make_scoped_ptr(message));
}

base::ProcessId ChannelMojo::GetPeerPID() const {
  return peer_pid_;
}

base::ProcessId ChannelMojo::GetSelfPID() const {
  return bootstrap_->GetSelfPID();
}

ChannelHandle ChannelMojo::TakePipeHandle() {
  return bootstrap_->TakePipeHandle();
}

void ChannelMojo::DidCreateChannel(mojo::embedder::ChannelInfo* info) {
  channel_info_.reset(info);
}

void ChannelMojo::OnMessageReceived(Message& message) {
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

#if defined(OS_POSIX) && !defined(OS_NACL)
int ChannelMojo::GetClientFileDescriptor() const {
  return bootstrap_->GetClientFileDescriptor();
}

int ChannelMojo::TakeClientFileDescriptor() {
  return bootstrap_->TakeClientFileDescriptor();
}
#endif  // defined(OS_POSIX) && !defined(OS_NACL)

}  // namespace IPC

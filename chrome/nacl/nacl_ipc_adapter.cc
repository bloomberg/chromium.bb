// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_ipc_adapter.h"

#include <limits.h>
#include <string.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "native_client/src/trusted/desc/nacl_desc_custom.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace {

enum BufferSizeStatus {
  // The buffer contains a full message with no extra bytes.
  MESSAGE_IS_COMPLETE,

  // The message doesn't fit and the buffer contains only some of it.
  MESSAGE_IS_TRUNCATED,

  // The buffer contains a full message + extra data.
  MESSAGE_HAS_EXTRA_DATA
};

BufferSizeStatus GetBufferStatus(const char* data, size_t len) {
  if (len < sizeof(NaClIPCAdapter::NaClMessageHeader))
    return MESSAGE_IS_TRUNCATED;

  const NaClIPCAdapter::NaClMessageHeader* header =
      reinterpret_cast<const NaClIPCAdapter::NaClMessageHeader*>(data);
  uint32 message_size =
      sizeof(NaClIPCAdapter::NaClMessageHeader) + header->payload_size;

  if (len == message_size)
    return MESSAGE_IS_COMPLETE;
  if (len > message_size)
    return MESSAGE_HAS_EXTRA_DATA;
  return MESSAGE_IS_TRUNCATED;
}

// This object allows the NaClDesc to hold a reference to a NaClIPCAdapter and
// forward calls to it.
struct DescThunker {
  explicit DescThunker(NaClIPCAdapter* adapter_param)
      : adapter(adapter_param) {
  }
  scoped_refptr<NaClIPCAdapter> adapter;
};

NaClIPCAdapter* ToAdapter(void* handle) {
  return static_cast<DescThunker*>(handle)->adapter.get();
}

// NaClDescCustom implementation.
void NaClDescCustomDestroy(void* handle) {
  delete static_cast<DescThunker*>(handle);
}

ssize_t NaClDescCustomSendMsg(void* handle, const NaClImcTypedMsgHdr* msg,
                              int /* flags */) {
  return static_cast<ssize_t>(ToAdapter(handle)->Send(msg));
}

ssize_t NaClDescCustomRecvMsg(void* handle, NaClImcTypedMsgHdr* msg,
                              int /* flags */) {
  return static_cast<ssize_t>(ToAdapter(handle)->BlockingReceive(msg));
}

NaClDesc* MakeNaClDescCustom(NaClIPCAdapter* adapter) {
  NaClDescCustomFuncs funcs = NACL_DESC_CUSTOM_FUNCS_INITIALIZER;
  funcs.Destroy = NaClDescCustomDestroy;
  funcs.SendMsg = NaClDescCustomSendMsg;
  funcs.RecvMsg = NaClDescCustomRecvMsg;
  // NaClDescMakeCustomDesc gives us a reference on the returned NaClDesc.
  return NaClDescMakeCustomDesc(new DescThunker(adapter), &funcs);
}

void DeleteChannel(IPC::Channel* channel) {
  delete channel;
}

// TODO(dmichael): Move all this handle conversion code to ppapi/proxy.
//                 crbug.com/165201
void WriteHandle(int handle_index,
                 const ppapi::proxy::SerializedHandle& handle,
                 IPC::Message* message) {
  ppapi::proxy::SerializedHandle::WriteHeader(handle.header(), message);

  // Now write the handle itself in POSIX style.
  message->WriteBool(true);  // valid == true
  message->WriteInt(handle_index);
}

typedef std::vector<ppapi::proxy::SerializedHandle> Handles;

// We define overload for catching SerializedHandles, so that we can share
// them correctly to the untrusted side, and another for handling all other
// parameters. See ConvertHandlesImpl for how these get used.
void ConvertHandlesInParam(const ppapi::proxy::SerializedHandle& handle,
                           Handles* handles,
                           IPC::Message* msg,
                           int* handle_index) {
  handles->push_back(handle);
  if (msg)
    WriteHandle((*handle_index)++, handle, msg);
}

// For PpapiMsg_ResourceReply and the reply to PpapiHostMsg_ResourceSyncCall,
// the handles are carried inside the ResourceMessageReplyParams.
// NOTE: We only translate handles from host->NaCl. The only kind of
//       ResourceMessageParams that travels this direction is
//       ResourceMessageReplyParams, so that's the only one we need to handle.
void ConvertHandlesInParam(
    const ppapi::proxy::ResourceMessageReplyParams& params,
    Handles* handles,
    IPC::Message* msg,
    int* handle_index) {
  // First, if we need to rewrite the message parameters, write everything
  // before the handles (there's nothing after the handles).
  if (msg) {
    params.WriteReplyHeader(msg);
    // IPC writes the vector length as an int before the contents of the
    // vector.
    msg->WriteInt(static_cast<int>(params.handles().size()));
  }
  for (Handles::const_iterator iter = params.handles().begin();
       iter != params.handles().end();
       ++iter) {
    // ConvertHandle will write each handle to |msg|, if necessary.
    ConvertHandlesInParam(*iter, handles, msg, handle_index);
  }
  // Tell ResourceMessageReplyParams that we have taken the handles, so it
  // shouldn't close them. The NaCl runtime will take ownership of them.
  params.ConsumeHandles();
}

// This overload is to catch all types other than SerializedHandle or
// ResourceMessageReplyParams. On Windows, |msg| will be a valid pointer, and we
// must write |param| to it.
template <class T>
void ConvertHandlesInParam(const T& param,
                           Handles* /* handles */,
                           IPC::Message* msg,
                           int* /* handle_index */) {
  // It's not a handle, so just write to the output message, if necessary.
  if (msg)
    IPC::WriteParam(msg, param);
}

// These just break apart the given tuple and run ConvertHandle over each param.
// The idea is to extract any handles in the tuple, while writing all data to
// msg (if msg is valid). The msg will only be valid on Windows, where we need
// to re-write all of the message parameters, writing the handles in POSIX style
// for NaCl.
template <class A>
void ConvertHandlesImpl(const Tuple1<A>& t1, Handles* handles,
                        IPC::Message* msg) {
  int handle_index = 0;
  ConvertHandlesInParam(t1.a, handles, msg, &handle_index);
}
template <class A, class B>
void ConvertHandlesImpl(const Tuple2<A, B>& t1, Handles* handles,
                        IPC::Message* msg) {
  int handle_index = 0;
  ConvertHandlesInParam(t1.a, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.b, handles, msg, &handle_index);
}
template <class A, class B, class C>
void ConvertHandlesImpl(const Tuple3<A, B, C>& t1, Handles* handles,
                        IPC::Message* msg) {
  int handle_index = 0;
  ConvertHandlesInParam(t1.a, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.b, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.c, handles, msg, &handle_index);
}
template <class A, class B, class C, class D>
void ConvertHandlesImpl(const Tuple4<A, B, C, D>& t1, Handles* handles,
                        IPC::Message* msg) {
  int handle_index = 0;
  ConvertHandlesInParam(t1.a, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.b, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.c, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.d, handles, msg, &handle_index);
}

template <class MessageType>
class HandleConverter {
 public:
  explicit HandleConverter(const IPC::Message* msg)
      : msg_(static_cast<const MessageType*>(msg)) {
  }
  bool ConvertMessage(Handles* handles, IPC::Message* out_msg) {
    typename TupleTypes<typename MessageType::Schema::Param>::ValueTuple params;
    if (!MessageType::Read(msg_, &params))
      return false;
    ConvertHandlesImpl(params, handles, out_msg);
    return true;
  }

  bool ConvertReply(Handles* handles, IPC::SyncMessage* out_msg) {
    typename TupleTypes<typename MessageType::Schema::ReplyParam>::ValueTuple
        params;
    if (!MessageType::ReadReplyParam(msg_, &params))
      return false;
    // If we need to rewrite the message (i.e., on Windows), we need to make
    // sure we write the message id first.
    if (out_msg) {
      out_msg->set_reply();
      int id = IPC::SyncMessage::GetMessageId(*msg_);
      out_msg->WriteInt(id);
    }
    ConvertHandlesImpl(params, handles, out_msg);
    return true;
  }
  // TODO(dmichael): Add ConvertSyncMessage for outgoing sync messages, if we
  //                 ever pass handles in one of those.

 private:
  const MessageType* msg_;
};

}  // namespace

class NaClIPCAdapter::RewrittenMessage
    : public base::RefCounted<RewrittenMessage> {
 public:
  RewrittenMessage();

  bool is_consumed() const { return data_read_cursor_ == data_len_; }

  void SetData(const NaClIPCAdapter::NaClMessageHeader& header,
               const void* payload, size_t payload_length);

  int Read(NaClImcTypedMsgHdr* msg);

  void AddDescriptor(nacl::DescWrapper* desc) { descs_.push_back(desc); }

  size_t desc_count() const { return descs_.size(); }

 private:
  friend class base::RefCounted<RewrittenMessage>;
  ~RewrittenMessage() {}

  scoped_array<char> data_;
  size_t data_len_;

  // Offset into data where the next read will happen. This will be equal to
  // data_len_ when all data has been consumed.
  size_t data_read_cursor_;

  // Wrapped descriptors for transfer to untrusted code.
  ScopedVector<nacl::DescWrapper> descs_;
};

NaClIPCAdapter::RewrittenMessage::RewrittenMessage()
    : data_len_(0),
      data_read_cursor_(0) {
}

void NaClIPCAdapter::RewrittenMessage::SetData(
    const NaClIPCAdapter::NaClMessageHeader& header,
    const void* payload,
    size_t payload_length) {
  DCHECK(!data_.get() && data_len_ == 0);
  size_t header_len = sizeof(NaClIPCAdapter::NaClMessageHeader);
  data_len_ = header_len + payload_length;
  data_.reset(new char[data_len_]);

  memcpy(data_.get(), &header, sizeof(NaClIPCAdapter::NaClMessageHeader));
  memcpy(&data_[header_len], payload, payload_length);
}

int NaClIPCAdapter::RewrittenMessage::Read(NaClImcTypedMsgHdr* msg) {
  CHECK(data_len_ >= data_read_cursor_);
  char* dest_buffer = static_cast<char*>(msg->iov[0].base);
  size_t dest_buffer_size = msg->iov[0].length;
  size_t bytes_to_write = std::min(dest_buffer_size,
                                   data_len_ - data_read_cursor_);
  if (bytes_to_write == 0)
    return 0;

  memcpy(dest_buffer, &data_[data_read_cursor_], bytes_to_write);
  data_read_cursor_ += bytes_to_write;

  // Once all data has been consumed, transfer any file descriptors.
  if (is_consumed()) {
    nacl_abi_size_t desc_count = static_cast<nacl_abi_size_t>(descs_.size());
    CHECK(desc_count <= msg->ndesc_length);
    msg->ndesc_length = desc_count;
    for (nacl_abi_size_t i = 0; i < desc_count; i++) {
      // Copy the NaClDesc to the buffer and add a ref so it won't be freed
      // when we clear our ScopedVector.
      msg->ndescv[i] = descs_[i]->desc();
      NaClDescRef(descs_[i]->desc());
    }
    descs_.clear();
  } else {
    msg->ndesc_length = 0;
  }
  return static_cast<int>(bytes_to_write);
}

NaClIPCAdapter::LockedData::LockedData()
    : channel_closed_(false) {
}

NaClIPCAdapter::LockedData::~LockedData() {
}

NaClIPCAdapter::IOThreadData::IOThreadData() {
}

NaClIPCAdapter::IOThreadData::~IOThreadData() {
}

NaClIPCAdapter::NaClIPCAdapter(const IPC::ChannelHandle& handle,
                               base::TaskRunner* runner)
    : lock_(),
      cond_var_(&lock_),
      task_runner_(runner),
      locked_data_() {
  io_thread_data_.channel_.reset(
      new IPC::Channel(handle, IPC::Channel::MODE_SERVER, this));
  // Note, we can not PostTask for ConnectChannelOnIOThread here. If we did,
  // and that task ran before this constructor completes, the reference count
  // would go to 1 and then to 0 because of the Task, before we've been returned
  // to the owning scoped_refptr, which is supposed to give us our first
  // ref-count.
}

NaClIPCAdapter::NaClIPCAdapter(scoped_ptr<IPC::Channel> channel,
                               base::TaskRunner* runner)
    : lock_(),
      cond_var_(&lock_),
      task_runner_(runner),
      locked_data_() {
  io_thread_data_.channel_ = channel.Pass();
}

void NaClIPCAdapter::ConnectChannel() {
  task_runner_->PostTask(FROM_HERE,
      base::Bind(&NaClIPCAdapter::ConnectChannelOnIOThread, this));
}

// Note that this message is controlled by the untrusted code. So we should be
// skeptical of anything it contains and quick to give up if anything is fishy.
int NaClIPCAdapter::Send(const NaClImcTypedMsgHdr* msg) {
  if (msg->iov_length != 1)
    return -1;

  base::AutoLock lock(lock_);

  const char* input_data = static_cast<char*>(msg->iov[0].base);
  size_t input_data_len = msg->iov[0].length;
  if (input_data_len > IPC::Channel::kMaximumMessageSize) {
    ClearToBeSent();
    return -1;
  }

  // current_message[_len] refers to the total input data received so far.
  const char* current_message;
  size_t current_message_len;
  bool did_append_input_data;
  if (locked_data_.to_be_sent_.empty()) {
    // No accumulated data, we can avoid a copy by referring to the input
    // buffer (the entire message fitting in one call is the common case).
    current_message = input_data;
    current_message_len = input_data_len;
    did_append_input_data = false;
  } else {
    // We've already accumulated some data, accumulate this new data and
    // point to the beginning of the buffer.

    // Make sure our accumulated message size doesn't overflow our max. Since
    // we know that data_len < max size (checked above) and our current
    // accumulated value is also < max size, we just need to make sure that
    // 2x max size can never overflow.
    COMPILE_ASSERT(IPC::Channel::kMaximumMessageSize < (UINT_MAX / 2),
                   MaximumMessageSizeWillOverflow);
    size_t new_size = locked_data_.to_be_sent_.size() + input_data_len;
    if (new_size > IPC::Channel::kMaximumMessageSize) {
      ClearToBeSent();
      return -1;
    }

    locked_data_.to_be_sent_.append(input_data, input_data_len);
    current_message = &locked_data_.to_be_sent_[0];
    current_message_len = locked_data_.to_be_sent_.size();
    did_append_input_data = true;
  }

  // Check the total data we've accumulated so far to see if it contains a full
  // message.
  switch (GetBufferStatus(current_message, current_message_len)) {
    case MESSAGE_IS_COMPLETE: {
      // Got a complete message, can send it out. This will be the common case.
      bool success = SendCompleteMessage(current_message, current_message_len);
      ClearToBeSent();
      return success ? static_cast<int>(input_data_len) : -1;
    }
    case MESSAGE_IS_TRUNCATED:
      // For truncated messages, just accumulate the new data (if we didn't
      // already do so above) and go back to waiting for more.
      if (!did_append_input_data)
        locked_data_.to_be_sent_.append(input_data, input_data_len);
      return static_cast<int>(input_data_len);
    case MESSAGE_HAS_EXTRA_DATA:
    default:
      // When the plugin gives us too much data, it's an error.
      ClearToBeSent();
      return -1;
  }
}

int NaClIPCAdapter::BlockingReceive(NaClImcTypedMsgHdr* msg) {
  if (msg->iov_length != 1)
    return -1;

  int retval = 0;
  {
    base::AutoLock lock(lock_);
    while (locked_data_.to_be_received_.empty() &&
           !locked_data_.channel_closed_)
      cond_var_.Wait();
    if (locked_data_.channel_closed_) {
      retval = -1;
    } else {
      retval = LockedReceive(msg);
      DCHECK(retval > 0);
    }
  }
  cond_var_.Signal();
  return retval;
}

void NaClIPCAdapter::CloseChannel() {
  {
    base::AutoLock lock(lock_);
    locked_data_.channel_closed_ = true;
  }
  cond_var_.Signal();

  task_runner_->PostTask(FROM_HERE,
      base::Bind(&NaClIPCAdapter::CloseChannelOnIOThread, this));
}

NaClDesc* NaClIPCAdapter::MakeNaClDesc() {
  return MakeNaClDescCustom(this);
}

#if defined(OS_POSIX)
int NaClIPCAdapter::TakeClientFileDescriptor() {
  return io_thread_data_.channel_->TakeClientFileDescriptor();
}
#endif

#define CASE_FOR_MESSAGE(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        HandleConverter<MESSAGE_TYPE> extractor(&msg); \
        if (!extractor.ConvertMessage(&handles, new_msg_ptr)) \
          return false; \
        break; \
      }
#define CASE_FOR_REPLY(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        HandleConverter<MESSAGE_TYPE> extractor(&msg); \
        if (!extractor.ConvertReply( \
                &handles, \
                static_cast<IPC::SyncMessage*>(new_msg_ptr))) \
          return false; \
        break; \
      }

bool NaClIPCAdapter::OnMessageReceived(const IPC::Message& msg) {
  {
    base::AutoLock lock(lock_);

    scoped_refptr<RewrittenMessage> rewritten_msg(new RewrittenMessage);

    // Pointer to the "new" message we will rewrite on Windows. On posix, this
    // isn't necessary, so it will stay NULL.
    IPC::Message* new_msg_ptr = NULL;
    IPC::Message new_msg(msg.routing_id(), msg.type(), msg.priority());
#if defined(OS_WIN)
    new_msg_ptr = &new_msg;
#else
    // Even on POSIX, we have to rewrite messages to create channels, because
    // these contain a handle with an invalid (place holder) descriptor. The
    // message sending code sees this and doesn't pass the descriptor over
    // correctly.
    if (msg.type() == PpapiMsg_CreateNaClChannel::ID)
      new_msg_ptr = &new_msg;
#endif

    Handles handles;
    switch (msg.type()) {
      CASE_FOR_MESSAGE(PpapiMsg_CreateNaClChannel)
      CASE_FOR_MESSAGE(PpapiMsg_PPBAudio_NotifyAudioStreamCreated)
      CASE_FOR_MESSAGE(PpapiPluginMsg_ResourceReply)
      case IPC_REPLY_ID: {
        int id = IPC::SyncMessage::GetMessageId(msg);
        LockedData::PendingSyncMsgMap::iterator iter(
            locked_data_.pending_sync_msgs_.find(id));
        if (iter == locked_data_.pending_sync_msgs_.end()) {
          NOTREACHED();
          return false;
        }
        uint32_t type = iter->second;
        locked_data_.pending_sync_msgs_.erase(iter);
        switch (type) {
          CASE_FOR_REPLY(PpapiHostMsg_PPBGraphics3D_GetTransferBuffer)
          CASE_FOR_REPLY(PpapiHostMsg_PPBImageData_CreateNaCl)
          CASE_FOR_REPLY(PpapiHostMsg_ResourceSyncCall)
          default:
            // Do nothing for messages we don't know.
            break;
        }
        break;
      }
      default:
        // Do nothing for messages we don't know.
        break;
    }
    // Now add any descriptors we found to rewritten_msg. |handles| is usually
    // empty, unless we read a message containing a FD or handle.
    nacl::DescWrapperFactory factory;
    for (Handles::const_iterator iter = handles.begin();
         iter != handles.end();
         ++iter) {
      scoped_ptr<nacl::DescWrapper> nacl_desc;
      switch (iter->type()) {
        case ppapi::proxy::SerializedHandle::SHARED_MEMORY: {
          const base::SharedMemoryHandle& shm_handle = iter->shmem();
          uint32_t size = iter->size();
          nacl_desc.reset(factory.ImportShmHandle(
#if defined(OS_WIN)
              reinterpret_cast<const NaClHandle>(shm_handle),
#else
              shm_handle.fd,
#endif
              static_cast<size_t>(size)));
          break;
        }
        case ppapi::proxy::SerializedHandle::SOCKET: {
          nacl_desc.reset(factory.ImportSyncSocketHandle(
#if defined(OS_WIN)
              reinterpret_cast<const NaClHandle>(iter->descriptor())
#else
              iter->descriptor().fd
#endif
          ));
          break;
        }
        case ppapi::proxy::SerializedHandle::CHANNEL_HANDLE: {
          // Check that this came from a PpapiMsg_CreateNaClChannel message.
          // This code here is only appropriate for that message.
          DCHECK(msg.type() == PpapiMsg_CreateNaClChannel::ID);
          IPC::ChannelHandle channel_handle =
              IPC::Channel::GenerateVerifiedChannelID("nacl");
          scoped_refptr<NaClIPCAdapter> ipc_adapter(
              new NaClIPCAdapter(channel_handle, task_runner_));
          ipc_adapter->ConnectChannel();
#if defined(OS_POSIX)
          channel_handle.socket = base::FileDescriptor(
              ipc_adapter->TakeClientFileDescriptor(), true);
#endif
          nacl_desc.reset(factory.MakeGeneric(ipc_adapter->MakeNaClDesc()));
          // Send back a message that the channel was created.
          scoped_ptr<IPC::Message> response(
              new PpapiHostMsg_ChannelCreated(channel_handle));
          task_runner_->PostTask(FROM_HERE,
              base::Bind(&NaClIPCAdapter::SendMessageOnIOThread, this,
                         base::Passed(&response)));
          break;
        }
        case ppapi::proxy::SerializedHandle::FILE:
          // TODO(raymes): Handle file handles for NaCl.
          NOTIMPLEMENTED();
          break;
        case ppapi::proxy::SerializedHandle::INVALID: {
          // Nothing to do. TODO(dmichael): Should we log this? Or is it
          // sometimes okay to pass an INVALID handle?
          break;
        }
        // No default, so the compiler will warn us if new types get added.
      }
      if (nacl_desc.get())
        rewritten_msg->AddDescriptor(nacl_desc.release());
    }
    if (new_msg_ptr && !handles.empty())
      SaveMessage(*new_msg_ptr, rewritten_msg.get());
    else
      SaveMessage(msg, rewritten_msg.get());
  }
  cond_var_.Signal();
  return true;
}

void NaClIPCAdapter::OnChannelConnected(int32 peer_pid) {
}

void NaClIPCAdapter::OnChannelError() {
  CloseChannel();
}

NaClIPCAdapter::~NaClIPCAdapter() {
  // Make sure the channel is deleted on the IO thread.
  task_runner_->PostTask(FROM_HERE,
      base::Bind(&DeleteChannel, io_thread_data_.channel_.release()));
}

int NaClIPCAdapter::LockedReceive(NaClImcTypedMsgHdr* msg) {
  lock_.AssertAcquired();

  if (locked_data_.to_be_received_.empty())
    return 0;
  scoped_refptr<RewrittenMessage> current =
      locked_data_.to_be_received_.front();

  int retval = current->Read(msg);

  // When a message is entirely consumed, remove if from the waiting queue.
  if (current->is_consumed())
    locked_data_.to_be_received_.pop();

  return retval;
}

bool NaClIPCAdapter::SendCompleteMessage(const char* buffer,
                                         size_t buffer_len) {
  // The message will have already been validated, so we know it's large enough
  // for our header.
  const NaClMessageHeader* header =
      reinterpret_cast<const NaClMessageHeader*>(buffer);

  // Length of the message not including the body. The data passed to us by the
  // plugin should match that in the message header. This should have already
  // been validated by GetBufferStatus.
  int body_len = static_cast<int>(buffer_len - sizeof(NaClMessageHeader));
  DCHECK(body_len == static_cast<int>(header->payload_size));

  // We actually discard the flags and only copy the ones we care about. This
  // is just because message doesn't have a constructor that takes raw flags.
  scoped_ptr<IPC::Message> msg(
      new IPC::Message(header->routing, header->type,
                       IPC::Message::PRIORITY_NORMAL));
  if (header->flags & IPC::Message::SYNC_BIT)
    msg->set_sync();
  if (header->flags & IPC::Message::REPLY_BIT)
    msg->set_reply();
  if (header->flags & IPC::Message::REPLY_ERROR_BIT)
    msg->set_reply_error();
  if (header->flags & IPC::Message::UNBLOCK_BIT)
    msg->set_unblock(true);

  msg->WriteBytes(&buffer[sizeof(NaClMessageHeader)], body_len);

  // Technically we didn't have to do any of the previous work in the lock. But
  // sometimes our buffer will point to the to_be_sent_ string which is
  // protected by the lock, and it's messier to factor Send() such that it can
  // unlock for us. Holding the lock for the message construction, which is
  // just some memcpys, shouldn't be a big deal.
  lock_.AssertAcquired();
  if (locked_data_.channel_closed_)
    return false;  // TODO(brettw) clean up handles here when we add support!

  // Store the type of all sync messages so that later we can translate the
  // reply if necessary.
  if (msg->is_sync()) {
    int id = IPC::SyncMessage::GetMessageId(*msg);
    locked_data_.pending_sync_msgs_[id] = msg->type();
  }
  // Actual send must be done on the I/O thread.
  task_runner_->PostTask(FROM_HERE,
      base::Bind(&NaClIPCAdapter::SendMessageOnIOThread, this,
                 base::Passed(&msg)));
  return true;
}

void NaClIPCAdapter::ClearToBeSent() {
  lock_.AssertAcquired();

  // Don't let the string keep its buffer behind our back.
  std::string empty;
  locked_data_.to_be_sent_.swap(empty);
}

void NaClIPCAdapter::ConnectChannelOnIOThread() {
  if (!io_thread_data_.channel_->Connect())
    NOTREACHED();
}

void NaClIPCAdapter::CloseChannelOnIOThread() {
  io_thread_data_.channel_->Close();
}

void NaClIPCAdapter::SendMessageOnIOThread(scoped_ptr<IPC::Message> message) {
  io_thread_data_.channel_->Send(message.release());
}

void NaClIPCAdapter::SaveMessage(const IPC::Message& msg,
                                 RewrittenMessage* rewritten_msg) {
  lock_.AssertAcquired();
  // There is some padding in this structure (the "padding" member is 16
  // bits but this then gets padded to 32 bits). We want to be sure not to
  // leak data to the untrusted plugin, so zero everything out first.
  NaClMessageHeader header;
  memset(&header, 0, sizeof(NaClMessageHeader));

  header.payload_size = static_cast<uint32>(msg.payload_size());
  header.routing = msg.routing_id();
  header.type = msg.type();
  header.flags = msg.flags();
  header.num_fds = static_cast<int>(rewritten_msg->desc_count());

  rewritten_msg->SetData(header, msg.payload(), msg.payload_size());
  locked_data_.to_be_received_.push(rewritten_msg);
}


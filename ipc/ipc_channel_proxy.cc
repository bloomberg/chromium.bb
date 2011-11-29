// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

// This helper ensures the message is deleted if the task is deleted without
// having been run.
class SendCallbackHelper
    : public base::RefCountedThreadSafe<SendCallbackHelper> {
 public:
  SendCallbackHelper(ChannelProxy::Context* context, Message* message)
      : context_(context),
        message_(message) {
  }

  void Send() {
    context_->OnSendMessage(message_.release());
  }

 private:
  scoped_refptr<ChannelProxy::Context> context_;
  scoped_ptr<Message> message_;

  DISALLOW_COPY_AND_ASSIGN(SendCallbackHelper);
};

//------------------------------------------------------------------------------

ChannelProxy::MessageFilter::MessageFilter() {}

ChannelProxy::MessageFilter::~MessageFilter() {}

void ChannelProxy::MessageFilter::OnFilterAdded(Channel* channel) {}

void ChannelProxy::MessageFilter::OnFilterRemoved() {}

void ChannelProxy::MessageFilter::OnChannelConnected(int32 peer_pid) {}

void ChannelProxy::MessageFilter::OnChannelError() {}

void ChannelProxy::MessageFilter::OnChannelClosing() {}

bool ChannelProxy::MessageFilter::OnMessageReceived(const Message& message) {
  return false;
}

void ChannelProxy::MessageFilter::OnDestruct() const {
  delete this;
}

//------------------------------------------------------------------------------

ChannelProxy::Context::Context(Channel::Listener* listener,
                               base::MessageLoopProxy* ipc_message_loop)
    : listener_message_loop_(base::MessageLoopProxy::current()),
      listener_(listener),
      ipc_message_loop_(ipc_message_loop),
      peer_pid_(0),
      channel_connected_called_(false) {
}

ChannelProxy::Context::~Context() {
}

void ChannelProxy::Context::CreateChannel(const IPC::ChannelHandle& handle,
                                          const Channel::Mode& mode) {
  DCHECK(channel_.get() == NULL);
  channel_id_ = handle.name;
  channel_.reset(new Channel(handle, mode, this));
}

bool ChannelProxy::Context::TryFilters(const Message& message) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::GetInstance();
  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i]->OnMessageReceived(message)) {
#ifdef IPC_MESSAGE_LOG_ENABLED
      if (logger->Enabled())
        logger->OnPostDispatchMessage(message, channel_id_);
#endif
      return true;
    }
  }
  return false;
}

// Called on the IPC::Channel thread
bool ChannelProxy::Context::OnMessageReceived(const Message& message) {
  // First give a chance to the filters to process this message.
  if (!TryFilters(message))
    OnMessageReceivedNoFilter(message);
  return true;
}

// Called on the IPC::Channel thread
bool ChannelProxy::Context::OnMessageReceivedNoFilter(const Message& message) {
  // NOTE: This code relies on the listener's message loop not going away while
  // this thread is active.  That should be a reasonable assumption, but it
  // feels risky.  We may want to invent some more indirect way of referring to
  // a MessageLoop if this becomes a problem.
  listener_message_loop_->PostTask(
      FROM_HERE, base::Bind(&Context::OnDispatchMessage, this, message));
  return true;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelConnected(int32 peer_pid) {
  // Add any pending filters.  This avoids a race condition where someone
  // creates a ChannelProxy, calls AddFilter, and then right after starts the
  // peer process.  The IO thread could receive a message before the task to add
  // the filter is run on the IO thread.
  OnAddFilter();

  peer_pid_ = peer_pid;
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelConnected(peer_pid);

  // See above comment about using listener_message_loop_ here.
  listener_message_loop_->PostTask(
      FROM_HERE, base::Bind(&Context::OnDispatchConnected, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelError() {
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // See above comment about using listener_message_loop_ here.
  listener_message_loop_->PostTask(
      FROM_HERE, base::Bind(&Context::OnDispatchError, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelOpened() {
  DCHECK(channel_ != NULL);

  // Assume a reference to ourselves on behalf of this thread.  This reference
  // will be released when we are closed.
  AddRef();

  if (!channel_->Connect()) {
    OnChannelError();
    return;
  }

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_.get());
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelClosed() {
  // It's okay for IPC::ChannelProxy::Close to be called more than once, which
  // would result in this branch being taken.
  if (!channel_.get())
    return;

  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }

  // We don't need the filters anymore.
  filters_.clear();

  channel_.reset();

  // Balance with the reference taken during startup.  This may result in
  // self-destruction.
  Release();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnSendMessage(Message* message) {
  if (!channel_.get()) {
    delete message;
    OnChannelClosed();
    return;
  }
  if (!channel_->Send(message))
    OnChannelError();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnAddFilter() {
  std::vector<scoped_refptr<MessageFilter> > new_filters;
  {
    base::AutoLock auto_lock(pending_filters_lock_);
    new_filters.swap(pending_filters_);
  }

  for (size_t i = 0; i < new_filters.size(); ++i) {
    filters_.push_back(new_filters[i]);

    // If the channel has already been created, then we need to send this
    // message so that the filter gets access to the Channel.
    if (channel_.get())
      new_filters[i]->OnFilterAdded(channel_.get());
    // Ditto for if the channel has been connected.
    if (peer_pid_)
      new_filters[i]->OnChannelConnected(peer_pid_);
  }
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnRemoveFilter(MessageFilter* filter) {
  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i].get() == filter) {
      filter->OnFilterRemoved();
      filters_.erase(filters_.begin() + i);
      return;
    }
  }

  NOTREACHED() << "filter to be removed not found";
}

// Called on the listener's thread
void ChannelProxy::Context::AddFilter(MessageFilter* filter) {
  base::AutoLock auto_lock(pending_filters_lock_);
  pending_filters_.push_back(make_scoped_refptr(filter));
  ipc_message_loop_->PostTask(
      FROM_HERE, base::Bind(&Context::OnAddFilter, this));
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchMessage(const Message& message) {
  if (!listener_)
    return;

  OnDispatchConnected();

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::GetInstance();
  if (message.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(message);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  listener_->OnMessageReceived(message);

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(message, channel_id_);
#endif
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchConnected() {
  if (channel_connected_called_)
    return;

  channel_connected_called_ = true;
  if (listener_)
    listener_->OnChannelConnected(peer_pid_);
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchError() {
  if (listener_)
    listener_->OnChannelError();
}

//-----------------------------------------------------------------------------

ChannelProxy::ChannelProxy(const IPC::ChannelHandle& channel_handle,
                           Channel::Mode mode,
                           Channel::Listener* listener,
                           base::MessageLoopProxy* ipc_thread)
    : context_(new Context(listener, ipc_thread)),
      outgoing_message_filter_(NULL),
      did_init_(false) {
  Init(channel_handle, mode, true);
}

ChannelProxy::ChannelProxy(Context* context)
    : context_(context),
      outgoing_message_filter_(NULL),
      did_init_(false) {
}

ChannelProxy::~ChannelProxy() {
  Close();
}

void ChannelProxy::Init(const IPC::ChannelHandle& channel_handle,
                        Channel::Mode mode,
                        bool create_pipe_now) {
  DCHECK(!did_init_);
#if defined(OS_POSIX)
  // When we are creating a server on POSIX, we need its file descriptor
  // to be created immediately so that it can be accessed and passed
  // to other processes. Forcing it to be created immediately avoids
  // race conditions that may otherwise arise.
  if (mode & Channel::MODE_SERVER_FLAG) {
    create_pipe_now = true;
  }
#endif  // defined(OS_POSIX)

  if (create_pipe_now) {
    // Create the channel immediately.  This effectively sets up the
    // low-level pipe so that the client can connect.  Without creating
    // the pipe immediately, it is possible for a listener to attempt
    // to connect and get an error since the pipe doesn't exist yet.
    context_->CreateChannel(channel_handle, mode);
  } else {
    context_->ipc_message_loop()->PostTask(
        FROM_HERE, base::Bind(&Context::CreateChannel, context_.get(),
                              channel_handle, mode));
  }

  // complete initialization on the background thread
  context_->ipc_message_loop()->PostTask(
      FROM_HERE, base::Bind(&Context::OnChannelOpened, context_.get()));

  did_init_ = true;
}

void ChannelProxy::Close() {
  // Clear the backpointer to the listener so that any pending calls to
  // Context::OnDispatchMessage or OnDispatchError will be ignored.  It is
  // possible that the channel could be closed while it is receiving messages!
  context_->Clear();

  if (context_->ipc_message_loop()) {
    context_->ipc_message_loop()->PostTask(
        FROM_HERE, base::Bind(&Context::OnChannelClosed, context_.get()));
  }
}

bool ChannelProxy::Send(Message* message) {
  DCHECK(did_init_);
  if (outgoing_message_filter())
    message = outgoing_message_filter()->Rewrite(message);

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::GetInstance()->OnSendMessage(message, context_->channel_id());
#endif

  context_->ipc_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SendCallbackHelper::Send,
                 new SendCallbackHelper(context_.get(), message)));
  return true;
}

void ChannelProxy::AddFilter(MessageFilter* filter) {
  context_->AddFilter(filter);
}

void ChannelProxy::RemoveFilter(MessageFilter* filter) {
  context_->ipc_message_loop()->PostTask(
      FROM_HERE, base::Bind(&Context::OnRemoveFilter, context_.get(),
                            make_scoped_refptr(filter)));
}

void ChannelProxy::ClearIPCMessageLoop() {
  context()->ClearIPCMessageLoop();
}

#if defined(OS_POSIX) && !defined(OS_NACL)
// See the TODO regarding lazy initialization of the channel in
// ChannelProxy::Init().
int ChannelProxy::GetClientFileDescriptor() {
  Channel* channel = context_.get()->channel_.get();
  // Channel must have been created first.
  DCHECK(channel) << context_.get()->channel_id_;
  return channel->GetClientFileDescriptor();
}

int ChannelProxy::TakeClientFileDescriptor() {
  Channel* channel = context_.get()->channel_.get();
  // Channel must have been created first.
  DCHECK(channel) << context_.get()->channel_id_;
  return channel->TakeClientFileDescriptor();
}

bool ChannelProxy::GetClientEuid(uid_t* client_euid) const {
  Channel* channel = context_.get()->channel_.get();
  // Channel must have been created first.
  DCHECK(channel) << context_.get()->channel_id_;
  return channel->GetClientEuid(client_euid);
}
#endif

//-----------------------------------------------------------------------------

}  // namespace IPC

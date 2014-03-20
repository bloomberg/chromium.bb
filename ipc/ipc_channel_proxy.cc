// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

//------------------------------------------------------------------------------

class ChannelProxy::Context::MessageFilterRouter {
 public:
  typedef std::vector<MessageFilter*> MessageFilters;

  MessageFilterRouter() {}
  ~MessageFilterRouter() {}

  void AddFilter(MessageFilter* filter) {
    // Determine if the filter should be applied to all messages, or only
    // messages of a certain class.
    std::vector<uint32> supported_message_classes;
    if (filter->GetSupportedMessageClasses(&supported_message_classes)) {
      DCHECK(!supported_message_classes.empty());
      for (size_t i = 0; i < supported_message_classes.size(); ++i) {
        const int message_class = supported_message_classes[i];
        DCHECK(ValidMessageClass(message_class));
        // Safely ignore repeated subscriptions to a given message class for the
        // current filter being added.
        if (!message_class_filters_[message_class].empty() &&
            message_class_filters_[message_class].back() == filter) {
          continue;
        }
        message_class_filters_[message_class].push_back(filter);
      }
    } else {
      global_filters_.push_back(filter);
    }
  }

  void RemoveFilter(MessageFilter* filter) {
    if (RemoveFilter(global_filters_, filter))
      return;

    for (size_t i = 0; i < arraysize(message_class_filters_); ++i)
      RemoveFilter(message_class_filters_[i], filter);
  }

  bool TryFilters(const Message& message) {
    if (TryFilters(global_filters_, message))
      return true;

    const int message_class = IPC_MESSAGE_CLASS(message);
    if (!ValidMessageClass(message_class))
      return false;

    return TryFilters(message_class_filters_[message_class], message);
  }

  void Clear() {
    global_filters_.clear();
    for (size_t i = 0; i < arraysize(message_class_filters_); ++i)
      message_class_filters_[i].clear();
  }

 private:
  static bool TryFilters(MessageFilters& filters, const IPC::Message& message) {
    for (size_t i = 0; i < filters.size(); ++i) {
      if (filters[i]->OnMessageReceived(message)) {
        return true;
      }
    }
    return false;
  }

  static bool RemoveFilter(MessageFilters& filters, MessageFilter* filter) {
    MessageFilters::iterator it =
        std::remove(filters.begin(), filters.end(), filter);
    if (it == filters.end())
      return false;

    filters.erase(it, filters.end());
    return true;
  }

  static bool ValidMessageClass(int message_class) {
    return message_class >= 0 && message_class < LastIPCMsgStart;
  }

  // List of global and selective filters; a given filter will exist in either
  // |message_global_filters_| OR |message_class_filters_|, but not both.
  // Note that |message_global_filters_| will be given first offering of any
  // given message.  It's the filter implementer and installer's
  // responsibility to ensure that a filter is either global or selective to
  // ensure proper message filtering order.
  MessageFilters global_filters_;
  MessageFilters message_class_filters_[LastIPCMsgStart];
};

//------------------------------------------------------------------------------

ChannelProxy::MessageFilter::MessageFilter() {}

void ChannelProxy::MessageFilter::OnFilterAdded(Channel* channel) {}

void ChannelProxy::MessageFilter::OnFilterRemoved() {}

void ChannelProxy::MessageFilter::OnChannelConnected(int32 peer_pid) {}

void ChannelProxy::MessageFilter::OnChannelError() {}

void ChannelProxy::MessageFilter::OnChannelClosing() {}

bool ChannelProxy::MessageFilter::OnMessageReceived(const Message& message) {
  return false;
}

bool ChannelProxy::MessageFilter::GetSupportedMessageClasses(
    std::vector<uint32>* /*supported_message_classes*/) const {
  return false;
}

ChannelProxy::MessageFilter::~MessageFilter() {}

//------------------------------------------------------------------------------

ChannelProxy::Context::Context(Listener* listener,
                               base::SingleThreadTaskRunner* ipc_task_runner)
    : listener_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      listener_(listener),
      ipc_task_runner_(ipc_task_runner),
      channel_connected_called_(false),
      message_filter_router_(new MessageFilterRouter()),
      peer_pid_(base::kNullProcessId) {
  DCHECK(ipc_task_runner_.get());
}

ChannelProxy::Context::~Context() {
}

void ChannelProxy::Context::ClearIPCTaskRunner() {
  ipc_task_runner_ = NULL;
}

void ChannelProxy::Context::CreateChannel(const IPC::ChannelHandle& handle,
                                          const Channel::Mode& mode) {
  DCHECK(!channel_);
  channel_id_ = handle.name;
  channel_.reset(new Channel(handle, mode, this));
}

bool ChannelProxy::Context::TryFilters(const Message& message) {
  DCHECK(message_filter_router_);
#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::GetInstance();
  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  if (message_filter_router_->TryFilters(message)) {
#ifdef IPC_MESSAGE_LOG_ENABLED
    if (logger->Enabled())
      logger->OnPostDispatchMessage(message, channel_id_);
#endif
    return true;
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
  listener_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Context::OnDispatchMessage, this, message));
  return true;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelConnected(int32 peer_pid) {
  // We cache off the peer_pid so it can be safely accessed from both threads.
  peer_pid_ = channel_->peer_pid();

  // Add any pending filters.  This avoids a race condition where someone
  // creates a ChannelProxy, calls AddFilter, and then right after starts the
  // peer process.  The IO thread could receive a message before the task to add
  // the filter is run on the IO thread.
  OnAddFilter();

  // See above comment about using listener_task_runner_ here.
  listener_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Context::OnDispatchConnected, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelError() {
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // See above comment about using listener_task_runner_ here.
  listener_task_runner_->PostTask(
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
  if (!channel_)
    return;

  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }

  // We don't need the filters anymore.
  message_filter_router_->Clear();
  filters_.clear();
  // We don't need the lock, because at this point, the listener thread can't
  // access it any more.
  pending_filters_.clear();

  channel_.reset();

  // Balance with the reference taken during startup.  This may result in
  // self-destruction.
  Release();
}

void ChannelProxy::Context::Clear() {
  listener_ = NULL;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnSendMessage(scoped_ptr<Message> message) {
  if (!channel_) {
    OnChannelClosed();
    return;
  }

  if (!channel_->Send(message.release()))
    OnChannelError();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnAddFilter() {
  // Our OnChannelConnected method has not yet been called, so we can't be
  // sure that channel_ is valid yet. When OnChannelConnected *is* called,
  // it invokes OnAddFilter, so any pending filter(s) will be added at that
  // time.
  if (peer_pid_ == base::kNullProcessId)
    return;

  std::vector<scoped_refptr<MessageFilter> > new_filters;
  {
    base::AutoLock auto_lock(pending_filters_lock_);
    new_filters.swap(pending_filters_);
  }

  for (size_t i = 0; i < new_filters.size(); ++i) {
    filters_.push_back(new_filters[i]);

    message_filter_router_->AddFilter(new_filters[i].get());

    // The channel has already been created and connected, so we need to
    // inform the filters right now.
    new_filters[i]->OnFilterAdded(channel_.get());
    new_filters[i]->OnChannelConnected(peer_pid_);
  }
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnRemoveFilter(MessageFilter* filter) {
  if (peer_pid_ == base::kNullProcessId) {
    // The channel is not yet connected, so any filters are still pending.
    base::AutoLock auto_lock(pending_filters_lock_);
    for (size_t i = 0; i < pending_filters_.size(); ++i) {
      if (pending_filters_[i].get() == filter) {
        filter->OnFilterRemoved();
        pending_filters_.erase(pending_filters_.begin() + i);
        return;
      }
    }
    return;
  }
  if (!channel_)
    return;  // The filters have already been deleted.

  message_filter_router_->RemoveFilter(filter);

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
  ipc_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Context::OnAddFilter, this));
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchMessage(const Message& message) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::GetInstance();
  std::string name;
  logger->GetMessageText(message.type(), &name, &message, NULL);
  TRACE_EVENT1("toplevel", "ChannelProxy::Context::OnDispatchMessage",
               "name", name);
#else
  TRACE_EVENT2("toplevel", "ChannelProxy::Context::OnDispatchMessage",
               "class", IPC_MESSAGE_ID_CLASS(message.type()),
               "line", IPC_MESSAGE_ID_LINE(message.type()));
#endif

  if (!listener_)
    return;

  OnDispatchConnected();

#ifdef IPC_MESSAGE_LOG_ENABLED
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
                           Listener* listener,
                           base::SingleThreadTaskRunner* ipc_task_runner)
    : context_(new Context(listener, ipc_task_runner)),
      did_init_(false) {
  Init(channel_handle, mode, true);
}

ChannelProxy::ChannelProxy(Context* context)
    : context_(context),
      did_init_(false) {
}

ChannelProxy::~ChannelProxy() {
  DCHECK(CalledOnValidThread());

  Close();
}

void ChannelProxy::Init(const IPC::ChannelHandle& channel_handle,
                        Channel::Mode mode,
                        bool create_pipe_now) {
  DCHECK(CalledOnValidThread());
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
    context_->ipc_task_runner()->PostTask(
        FROM_HERE, base::Bind(&Context::CreateChannel, context_.get(),
                              channel_handle, mode));
  }

  // complete initialization on the background thread
  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::Bind(&Context::OnChannelOpened, context_.get()));

  did_init_ = true;
}

void ChannelProxy::Close() {
  DCHECK(CalledOnValidThread());

  // Clear the backpointer to the listener so that any pending calls to
  // Context::OnDispatchMessage or OnDispatchError will be ignored.  It is
  // possible that the channel could be closed while it is receiving messages!
  context_->Clear();

  if (context_->ipc_task_runner()) {
    context_->ipc_task_runner()->PostTask(
        FROM_HERE, base::Bind(&Context::OnChannelClosed, context_.get()));
  }
}

bool ChannelProxy::Send(Message* message) {
  DCHECK(did_init_);

  // TODO(alexeypa): add DCHECK(CalledOnValidThread()) here. Currently there are
  // tests that call Send() from a wrong thread. See http://crbug.com/163523.

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::GetInstance()->OnSendMessage(message, context_->channel_id());
#endif

  context_->ipc_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChannelProxy::Context::OnSendMessage,
                 context_, base::Passed(scoped_ptr<Message>(message))));
  return true;
}

void ChannelProxy::AddFilter(MessageFilter* filter) {
  DCHECK(CalledOnValidThread());

  context_->AddFilter(filter);
}

void ChannelProxy::RemoveFilter(MessageFilter* filter) {
  DCHECK(CalledOnValidThread());

  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::Bind(&Context::OnRemoveFilter, context_.get(),
                            make_scoped_refptr(filter)));
}

void ChannelProxy::ClearIPCTaskRunner() {
  DCHECK(CalledOnValidThread());

  context()->ClearIPCTaskRunner();
}

#if defined(OS_POSIX) && !defined(OS_NACL)
// See the TODO regarding lazy initialization of the channel in
// ChannelProxy::Init().
int ChannelProxy::GetClientFileDescriptor() {
  DCHECK(CalledOnValidThread());

  Channel* channel = context_.get()->channel_.get();
  // Channel must have been created first.
  DCHECK(channel) << context_.get()->channel_id_;
  return channel->GetClientFileDescriptor();
}

int ChannelProxy::TakeClientFileDescriptor() {
  DCHECK(CalledOnValidThread());

  Channel* channel = context_.get()->channel_.get();
  // Channel must have been created first.
  DCHECK(channel) << context_.get()->channel_id_;
  return channel->TakeClientFileDescriptor();
}

bool ChannelProxy::GetPeerEuid(uid_t* peer_euid) const {
  DCHECK(CalledOnValidThread());

  Channel* channel = context_.get()->channel_.get();
  // Channel must have been created first.
  DCHECK(channel) << context_.get()->channel_id_;
  return channel->GetPeerEuid(peer_euid);
}
#endif

//-----------------------------------------------------------------------------

}  // namespace IPC

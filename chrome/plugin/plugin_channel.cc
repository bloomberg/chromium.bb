// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/plugin_channel.h"

#include "base/command_line.h"
#include "base/lock.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/plugin/plugin_thread.h"
#include "chrome/plugin/webplugin_delegate_stub.h"
#include "chrome/plugin/webplugin_proxy.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

class PluginReleaseTask : public Task {
 public:
  void Run() {
    ChildProcess::current()->ReleaseProcess();
  }
};

// How long we wait before releasing the plugin process.
static const int kPluginReleaseTimeMS = 10000;


// If a sync call to the renderer results in a modal dialog, we need to have a
// way to know so that we can run a nested message loop to simulate what would
// happen in a single process browser and avoid deadlock.
class PluginChannel::MessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  MessageFilter() : channel_(NULL) { }
  ~MessageFilter() {
    // Clean up in case of renderer crash.
    for (ModalDialogEventMap::iterator i = modal_dialog_event_map_.begin();
        i != modal_dialog_event_map_.end(); ++i) {
      delete i->second.event;
    }
  }

  base::WaitableEvent* GetModalDialogEvent(
      gfx::NativeViewId containing_window) {
    AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (!modal_dialog_event_map_.count(containing_window)) {
      NOTREACHED();
      return NULL;
    }

    return modal_dialog_event_map_[containing_window].event;
  }

  // Decrement the ref count associated with the modal dialog event for the
  // given tab.
  void ReleaseModalDialogEvent(gfx::NativeViewId containing_window) {
    AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (!modal_dialog_event_map_.count(containing_window)) {
      NOTREACHED();
      return;
    }

    if (--(modal_dialog_event_map_[containing_window].refcount))
      return;

    // Delete the event when the stack unwinds as it could be in use now.
    MessageLoop::current()->DeleteSoon(
        FROM_HERE, modal_dialog_event_map_[containing_window].event);
    modal_dialog_event_map_.erase(containing_window);
  }

  bool Send(IPC::Message* message) {
    // Need this function for the IPC_MESSAGE_HANDLER_DELAY_REPLY macro.
    return channel_->Send(message);
  }

 private:
  void OnFilterAdded(IPC::Channel* channel) { channel_ = channel; }

  bool OnMessageReceived(const IPC::Message& message) {
    IPC_BEGIN_MESSAGE_MAP(PluginChannel::MessageFilter, message)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(PluginMsg_Init, OnInit)
      IPC_MESSAGE_HANDLER(PluginMsg_SignalModalDialogEvent,
                          OnSignalModalDialogEvent)
      IPC_MESSAGE_HANDLER(PluginMsg_ResetModalDialogEvent,
                          OnResetModalDialogEvent)
    IPC_END_MESSAGE_MAP()
    return message.type() == PluginMsg_SignalModalDialogEvent::ID ||
           message.type() == PluginMsg_ResetModalDialogEvent::ID;
  }

  void OnInit(const PluginMsg_Init_Params& params, IPC::Message* reply_msg) {
    AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (modal_dialog_event_map_.count(params.containing_window)) {
      modal_dialog_event_map_[params.containing_window].refcount++;
      return;
    }

    WaitableEventWrapper wrapper;
    wrapper.event = new base::WaitableEvent(true, false);
    wrapper.refcount = 1;
    modal_dialog_event_map_[params.containing_window] = wrapper;
  }

  void OnSignalModalDialogEvent(gfx::NativeViewId containing_window) {
    AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (modal_dialog_event_map_.count(containing_window))
      modal_dialog_event_map_[containing_window].event->Signal();
  }

  void OnResetModalDialogEvent(gfx::NativeViewId containing_window) {
    AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (modal_dialog_event_map_.count(containing_window))
      modal_dialog_event_map_[containing_window].event->Reset();
  }

  struct WaitableEventWrapper {
    base::WaitableEvent* event;
    int refcount;  // There could be multiple plugin instances per tab.
  };
  typedef std::map<gfx::NativeViewId, WaitableEventWrapper> ModalDialogEventMap;
  ModalDialogEventMap modal_dialog_event_map_;
  Lock modal_dialog_event_map_lock_;

  IPC::Channel* channel_;
};


PluginChannel* PluginChannel::GetPluginChannel(int renderer_id,
                                               MessageLoop* ipc_message_loop) {
  // Map renderer ID to a (single) channel to that process.
  std::string channel_name = StringPrintf(
      "%d.r%d", base::GetCurrentProcId(), renderer_id);

  PluginChannel* channel =
      static_cast<PluginChannel*>(PluginChannelBase::GetChannel(
          channel_name,
          IPC::Channel::MODE_SERVER,
          ClassFactory,
          ipc_message_loop,
          false));

  if (channel)
    channel->renderer_id_ = renderer_id;

  return channel;
}

PluginChannel::PluginChannel()
    : renderer_handle_(0),
      renderer_id_(-1),
#if defined(OS_POSIX)
      renderer_fd_(-1),
#endif
      in_send_(0),
      off_the_record_(false),
      filter_(new MessageFilter()) {
  SendUnblockingOnlyDuringSyncDispatch();
  ChildProcess::current()->AddRefProcess();
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
}

PluginChannel::~PluginChannel() {
  if (renderer_handle_)
    base::CloseProcessHandle(renderer_handle_);
#if defined(OS_POSIX)
  // If we still have the renderer FD, close it.
  if (renderer_fd_ != -1) {
    close(renderer_fd_);
  }
#endif
  MessageLoop::current()->PostDelayedTask(FROM_HERE, new PluginReleaseTask(),
                                          kPluginReleaseTimeMS);
}

bool PluginChannel::Send(IPC::Message* msg) {
  in_send_++;
  if (log_messages_) {
    LOG(INFO) << "sending message @" << msg << " on channel @" << this
              << " with type " << msg->type();
  }
  bool result = PluginChannelBase::Send(msg);
  in_send_--;
  return result;
}

void PluginChannel::OnMessageReceived(const IPC::Message& msg) {
  if (log_messages_) {
    LOG(INFO) << "received message @" << &msg << " on channel @" << this
              << " with type " << msg.type();
  }
  PluginChannelBase::OnMessageReceived(msg);
}

void PluginChannel::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginChannel, msg)
    IPC_MESSAGE_HANDLER(PluginMsg_CreateInstance, OnCreateInstance)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PluginMsg_DestroyInstance,
                                    OnDestroyInstance)
    IPC_MESSAGE_HANDLER(PluginMsg_GenerateRouteID, OnGenerateRouteID)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void PluginChannel::OnCreateInstance(const std::string& mime_type,
                                     int* instance_id) {
  *instance_id = GenerateRouteID();
  scoped_refptr<WebPluginDelegateStub> stub = new WebPluginDelegateStub(
      mime_type, *instance_id, this);
  AddRoute(*instance_id, stub, false);
  plugin_stubs_.push_back(stub);
}

void PluginChannel::OnDestroyInstance(int instance_id,
                                      IPC::Message* reply_msg) {
  for (size_t i = 0; i < plugin_stubs_.size(); ++i) {
    if (plugin_stubs_[i]->instance_id() == instance_id) {
      scoped_refptr<MessageFilter> filter(filter_);
      gfx::NativeViewId window =
          plugin_stubs_[i]->webplugin()->containing_window();
      plugin_stubs_.erase(plugin_stubs_.begin() + i);
      Send(reply_msg);
      RemoveRoute(instance_id);
      // NOTE: *this* might be deleted as a result of calling RemoveRoute.
      // Don't release the modal dialog event right away, but do it after the
      // stack unwinds since the plugin can be destroyed later if it's in use
      // right now.
      MessageLoop::current()->PostNonNestableTask(FROM_HERE, NewRunnableMethod(
          filter.get(), &MessageFilter::ReleaseModalDialogEvent, window));
      return;
    }
  }

  NOTREACHED() << "Couldn't find WebPluginDelegateStub to destroy";
}

void PluginChannel::OnGenerateRouteID(int* route_id) {
  *route_id = GenerateRouteID();
}

int PluginChannel::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

base::WaitableEvent* PluginChannel::GetModalDialogEvent(
    gfx::NativeViewId containing_window) {
  return filter_->GetModalDialogEvent(containing_window);
}

void PluginChannel::OnChannelConnected(int32 peer_pid) {
  base::ProcessHandle handle;
  if (!base::OpenProcessHandle(peer_pid, &handle)) {
    NOTREACHED();
  }
  renderer_handle_ = handle;
  PluginChannelBase::OnChannelConnected(peer_pid);
}

void PluginChannel::OnChannelError() {
  base::CloseProcessHandle(renderer_handle_);
  renderer_handle_ = 0;
  PluginChannelBase::OnChannelError();
  CleanUp();
}

void PluginChannel::CleanUp() {
  // We need to clean up the stubs so that they call NPPDestroy.  This will
  // also lead to them releasing their reference on this object so that it can
  // be deleted.
  for (size_t i = 0; i < plugin_stubs_.size(); ++i)
    RemoveRoute(plugin_stubs_[i]->instance_id());

  // Need to addref this object temporarily because otherwise removing the last
  // stub will cause the destructor of this object to be called, however at
  // that point plugin_stubs_ will have one element and its destructor will be
  // called twice.
  scoped_refptr<PluginChannel> me(this);

  plugin_stubs_.clear();
}

bool PluginChannel::Init(MessageLoop* ipc_message_loop, bool create_pipe_now) {
#if defined(OS_POSIX)
  // This gets called when the PluginChannel is initially created. At this
  // point, create the socketpair and assign the plugin side FD to the channel
  // name. Keep the renderer side FD as a member variable in the PluginChannel
  // to be able to transmit it through IPC.
  int plugin_fd;
  IPC::SocketPair(&plugin_fd, &renderer_fd_);
  IPC::AddChannelSocket(channel_name(), plugin_fd);
#endif
  if (!PluginChannelBase::Init(ipc_message_loop, create_pipe_now))
    return false;

  channel_->AddFilter(filter_.get());
  return true;
}

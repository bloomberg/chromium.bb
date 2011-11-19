// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/plugin/plugin_channel.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/plugin_messages.h"
#include "content/public/common/content_switches.h"
#include "content/plugin/plugin_thread.h"
#include "content/plugin/webplugin_delegate_stub.h"
#include "content/plugin/webplugin_proxy.h"
#include "webkit/plugins/npapi/plugin_instance.h"

#if defined(OS_POSIX)
#include "base/eintr_wrapper.h"
#include "ipc/ipc_channel_posix.h"
#endif

namespace {

class PluginReleaseTask : public Task {
 public:
  void Run() {
    ChildProcess::current()->ReleaseProcess();
  }
};

// How long we wait before releasing the plugin process.
const int kPluginReleaseTimeMs = 5 * 60 * 1000;  // 5 minutes

}  // namespace

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
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (!modal_dialog_event_map_.count(containing_window)) {
      NOTREACHED();
      return NULL;
    }

    return modal_dialog_event_map_[containing_window].event;
  }

  // Decrement the ref count associated with the modal dialog event for the
  // given tab.
  void ReleaseModalDialogEvent(gfx::NativeViewId containing_window) {
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
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
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
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
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (modal_dialog_event_map_.count(containing_window))
      modal_dialog_event_map_[containing_window].event->Signal();
  }

  void OnResetModalDialogEvent(gfx::NativeViewId containing_window) {
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (modal_dialog_event_map_.count(containing_window))
      modal_dialog_event_map_[containing_window].event->Reset();
  }

  struct WaitableEventWrapper {
    base::WaitableEvent* event;
    int refcount;  // There could be multiple plugin instances per tab.
  };
  typedef std::map<gfx::NativeViewId, WaitableEventWrapper> ModalDialogEventMap;
  ModalDialogEventMap modal_dialog_event_map_;
  base::Lock modal_dialog_event_map_lock_;

  IPC::Channel* channel_;
};

PluginChannel* PluginChannel::GetPluginChannel(
    int renderer_id, base::MessageLoopProxy* ipc_message_loop) {
  // Map renderer ID to a (single) channel to that process.
  std::string channel_key = StringPrintf(
      "%d.r%d", base::GetCurrentProcId(), renderer_id);

  PluginChannel* channel =
      static_cast<PluginChannel*>(NPChannelBase::GetChannel(
          channel_key,
          IPC::Channel::MODE_SERVER,
          ClassFactory,
          ipc_message_loop,
          false,
          ChildProcess::current()->GetShutDownEvent()));

  if (channel)
    channel->renderer_id_ = renderer_id;

  return channel;
}

// static
void PluginChannel::NotifyRenderersOfPendingShutdown() {
  Broadcast(new PluginHostMsg_PluginShuttingDown());
}

PluginChannel::PluginChannel()
    : renderer_handle_(0),
      renderer_id_(-1),
      in_send_(0),
      incognito_(false),
      filter_(new MessageFilter()) {
  set_send_unblocking_only_during_unblock_dispatch();
  ChildProcess::current()->AddRefProcess();
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
}

PluginChannel::~PluginChannel() {
  if (renderer_handle_)
    base::CloseProcessHandle(renderer_handle_);

  MessageLoop::current()->PostDelayedTask(FROM_HERE, new PluginReleaseTask(),
                                          kPluginReleaseTimeMs);
}

bool PluginChannel::Send(IPC::Message* msg) {
  in_send_++;
  if (log_messages_) {
    VLOG(1) << "sending message @" << msg << " on channel @" << this
            << " with type " << msg->type();
  }
  bool result = NPChannelBase::Send(msg);
  in_send_--;
  return result;
}

bool PluginChannel::OnMessageReceived(const IPC::Message& msg) {
  if (log_messages_) {
    VLOG(1) << "received message @" << &msg << " on channel @" << this
            << " with type " << msg.type();
  }
  return NPChannelBase::OnMessageReceived(msg);
}

bool PluginChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginChannel, msg)
    IPC_MESSAGE_HANDLER(PluginMsg_CreateInstance, OnCreateInstance)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PluginMsg_DestroyInstance,
                                    OnDestroyInstance)
    IPC_MESSAGE_HANDLER(PluginMsg_GenerateRouteID, OnGenerateRouteID)
    IPC_MESSAGE_HANDLER(PluginMsg_ClearSiteData, OnClearSiteData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PluginChannel::OnCreateInstance(const std::string& mime_type,
                                     int* instance_id) {
  *instance_id = GenerateRouteID();
  scoped_refptr<WebPluginDelegateStub> stub(new WebPluginDelegateStub(
      mime_type, *instance_id, this));
  AddRoute(*instance_id, stub, NULL);
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
      MessageLoop::current()->PostNonNestableTask(FROM_HERE, base::Bind(
          &MessageFilter::ReleaseModalDialogEvent, filter.get(), window));
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

void PluginChannel::OnClearSiteData(const std::string& site,
                                    uint64 flags,
                                    base::Time begin_time) {
  bool success = false;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  FilePath path = command_line->GetSwitchValuePath(switches::kPluginPath);
  scoped_refptr<webkit::npapi::PluginLib> plugin_lib(
      webkit::npapi::PluginLib::CreatePluginLib(path));
  if (plugin_lib.get()) {
    NPError err = plugin_lib->NP_Initialize();
    if (err == NPERR_NO_ERROR) {
      const char* site_str = site.empty() ? NULL : site.c_str();
      uint64 max_age;
      if (begin_time > base::Time()) {
        base::TimeDelta delta = base::Time::Now() - begin_time;
        max_age = delta.InSeconds();
      } else {
        max_age = kuint64max;
      }
      err = plugin_lib->NP_ClearSiteData(site_str, flags, max_age);
      std::string site_name =
          site.empty() ? "NULL"
                       : base::StringPrintf("\"%s\"", site_str);
      VLOG(1) << "NPP_ClearSiteData(" << site_name << ", " << flags << ", "
              << max_age << ") returned " << err;
      success = (err == NPERR_NO_ERROR);
    }
  }
  Send(new PluginHostMsg_ClearSiteDataResult(success));
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
  NPChannelBase::OnChannelConnected(peer_pid);
}

void PluginChannel::OnChannelError() {
  base::CloseProcessHandle(renderer_handle_);
  renderer_handle_ = 0;
  NPChannelBase::OnChannelError();
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

bool PluginChannel::Init(base::MessageLoopProxy* ipc_message_loop,
                         bool create_pipe_now,
                         base::WaitableEvent* shutdown_event) {
  if (!NPChannelBase::Init(ipc_message_loop, create_pipe_now, shutdown_event))
    return false;

  channel_->AddFilter(filter_.get());
  return true;
}


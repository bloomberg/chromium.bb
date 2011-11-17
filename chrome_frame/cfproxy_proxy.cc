// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/cfproxy_private.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/tuple.h"
#include "ipc/ipc_sync_message.h"
#include "chrome/common/automation_messages.h"

CFProxy::CFProxy(CFProxyTraits* api) : ipc_thread_("ipc"),
                                       sync_dispatcher_(&tab2delegate_),
                                       ipc_sender_(NULL),
                                       api_(api),
                                       delegate_count_(0),
                                       is_connected_(false) {
}

CFProxy::~CFProxy() {
  ipc_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CFProxy::CleanupOnIoThread, base::Unretained(this)));
  // ipc_thread destructor will do the Stop anyway. this is for debug :)
  ipc_thread_.Stop();
}

void CFProxy::Init(const ProxyParams& params) {
  ipc_thread_.StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
  ipc_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CFProxy::InitInIoThread, base::Unretained(this), params));
}

int CFProxy::AddDelegate(ChromeProxyDelegate* delegate) {
  ipc_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&CFProxy::AddDelegateOnIoThread,
                            base::Unretained(this), delegate));
  return ++delegate_count_;
}

int CFProxy::RemoveDelegate(ChromeProxyDelegate* delegate) {
  ipc_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&CFProxy::RemoveDelegateOnIoThread,
                            base::Unretained(this), delegate));
  return --delegate_count_;
}

void CFProxy::AddDelegateOnIoThread(ChromeProxyDelegate* delegate) {
  DCHECK(CalledOnIpcThread());
  DelegateHolder::AddDelegate(delegate);
  if (is_connected_) {
    delegate->Connected(this);
  }
}

void CFProxy::RemoveDelegateOnIoThread(ChromeProxyDelegate* delegate) {
  DCHECK(CalledOnIpcThread());
  // Cancel any calls in progress.
  sync_dispatcher_.Cancel(delegate);
  DelegateHolder::RemoveDelegate(delegate);
  delegate->Disconnected();
}

void CFProxy::InitInIoThread(const ProxyParams& params) {
  DCHECK(CalledOnIpcThread());
  std::string channel_id = GenerateChannelId();
  ipc_sender_ = api_->CreateChannel(channel_id, this);
  std::wstring cmd_line = BuildCmdLine(channel_id, params.profile_path,
                                       params.extra_params);
  if (!cmd_line.empty() && api_->LaunchApp(cmd_line)) {
    ipc_thread_.message_loop()->PostDelayedTask(
        FROM_HERE, base::Bind(&CFProxy::LaunchTimeOut, base::Unretained(this)),
        params.timeout.InMilliseconds());
  } else {
    OnPeerLost(ChromeProxyDelegate::CHROME_EXE_LAUNCH_FAILED);
  }
}

void CFProxy::CleanupOnIoThread() {
  DCHECK(CalledOnIpcThread());
  if (ipc_sender_) {
    api_->CloseChannel(ipc_sender_);
    ipc_sender_ = NULL;
  }
  // TODO(stoyan): shall we notify delegates?
  // The object is dying, so under normal circumstances there should be
  // no delegates.
  DCHECK_EQ(0, delegate_count_);
  DCHECK_EQ(0u, delegate_list_.size());
  DCHECK_EQ(0u, tab2delegate_.size());
}

void CFProxy::LaunchTimeOut() {
  DCHECK(CalledOnIpcThread());
  if (!is_connected_) {
    OnPeerLost(ChromeProxyDelegate::CHROME_EXE_LAUNCH_TIMEOUT);
  }
}

void CFProxy::OnPeerLost(ChromeProxyDelegate::DisconnectReason reason) {
  // Kill the channel. Inform delegates
  DCHECK(CalledOnIpcThread());
  if (ipc_sender_) {
    api_->CloseChannel(ipc_sender_);
    ipc_sender_ = NULL;
  }

  for (DelegateList::iterator it = delegate_list_.begin();
       it != delegate_list_.end(); ++it) {
    (*it)->PeerLost(this, reason);
  }
}

void CFProxy::SendIpcMessage(IPC::Message* m) {
  ipc_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&CFProxy::SendIpcMessageOnIoThread,
                            base::Unretained(this), m));
}

void CFProxy::SendIpcMessageOnIoThread(IPC::Message* m) {
  DCHECK(CalledOnIpcThread());
  if (ipc_sender_) {
    ipc_sender_->Send(m);
  } else {
    delete m;
  }
}

//////////////////////////////////////////////////////////////////////////
// Sync messages.
void CFProxy::Tab_Find(int tab, const string16& search_string,
                       FindInPageDirection forward, FindInPageCase match_case,
                       bool find_next) {
  AutomationMsg_Find_Params params;
  params.search_string = search_string;
  params.find_next = find_next;
  params.match_case = (match_case == CASE_SENSITIVE);
  params.forward = (forward == FWD);
  IPC::SyncMessage* m = new AutomationMsg_Find(tab, params, NULL, NULL);
  // Not interested in result.
  sync_dispatcher_.QueueSyncMessage(m, NULL, NULL);
  SendIpcMessage(m);
}

void CFProxy::Tab_OverrideEncoding(int tab, const char* encoding) {
  IPC::SyncMessage* m = new AutomationMsg_OverrideEncoding(tab, encoding, NULL);
  // Not interested in result.
  sync_dispatcher_.QueueSyncMessage(m, NULL, NULL);
  SendIpcMessage(m);
}

void CFProxy::Tab_Navigate(int tab, const GURL& url, const GURL& referrer) {
  IPC::SyncMessage* m = new AutomationMsg_NavigateInExternalTab(
      tab, url, referrer, NULL);
  // We probably are not interested in result since provider just checks
  // whether tab handle is valid.
  sync_dispatcher_.QueueSyncMessage(m, NULL, NULL);
  SendIpcMessage(m);
}

void CFProxy::CreateTab(ChromeProxyDelegate* delegate,
                        const ExternalTabSettings& p) {
  IPC::SyncMessage* m = new AutomationMsg_CreateExternalTab(p, 0, 0, 0, 0);
  sync_dispatcher_.QueueSyncMessage(m, delegate, NULL);
  SendIpcMessage(m);
}

void CFProxy::ConnectTab(ChromeProxyDelegate* delegate, HWND hwnd,
                         uint64 cookie) {
  IPC::SyncMessage* m = new AutomationMsg_ConnectExternalTab(cookie, true,
      hwnd, NULL, NULL, NULL, 0);
  sync_dispatcher_.QueueSyncMessage(m, delegate, NULL);
  SendIpcMessage(m);
}

void CFProxy::BlockTab(uint64 cookie) {
  IPC::SyncMessage* m = new AutomationMsg_ConnectExternalTab(cookie, false,
      NULL, NULL, NULL, NULL, 0);
  sync_dispatcher_.QueueSyncMessage(m, NULL, NULL);
  SendIpcMessage(m);
}

void CFProxy::Tab_RunUnloadHandlers(int tab) {
  IPC::SyncMessage* m = new AutomationMsg_RunUnloadHandlers(tab, 0);
  ChromeProxyDelegate* p = Tab2Delegate(tab);
  sync_dispatcher_.QueueSyncMessage(m, p, NULL);
  SendIpcMessage(m);
}

// IPC::Channel::Listener
bool CFProxy::OnMessageReceived(const IPC::Message& message) {
  // Handle sync message reply.
  bool done = sync_dispatcher_.OnReplyReceived(&message);
  if (done)
    return true;

  // Handle tab related message.
  ChromeProxyDelegate* d = Tab2Delegate(message.routing_id());
  if (d)
    return d->OnMessageReceived(message);

  DLOG(WARNING) << "Unknown message received!";
  return false;
}

void CFProxy::OnChannelConnected(int32 peer_pid) {
  is_connected_ = true;
  // TODO(stoyan): May be we should wait for Hello message.
  for (DelegateList::iterator it = delegate_list_.begin();
       it != delegate_list_.end(); ++it) {
    (*it)->Connected(this);
  }
}

void CFProxy::OnChannelError() {
  is_connected_ = false;

  // Inform the sync message callbacks that there are not going to see
  // any reply.
  sync_dispatcher_.OnChannelClosed();
  OnPeerLost(ChromeProxyDelegate::CHANNEL_ERROR);

  // TODO(stoyan): Relaunch?
}

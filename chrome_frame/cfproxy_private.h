// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CFPROXY_PRIVATE_H_
#define CHROME_FRAME_CFPROXY_PRIVATE_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "chrome_frame/cfproxy.h"
#include "base/threading/thread.h"
// Since we can't forward declare IPC::Message::Sender or IPC::Channel::Listener
#include "ipc/ipc_message.h"
#include "ipc/ipc_channel.h"

typedef std::map<int, ChromeProxyDelegate*> TabsMap;

// This is the functions needed by CFProxy implementation.
// Extracted in separate class so we can mock it.
class CFProxyTraits {
 public:
  virtual IPC::Message::Sender* CreateChannel(const std::string& id,
                                              IPC::Channel::Listener* l);
  virtual void CloseChannel(IPC::Message::Sender* s);
  virtual bool LaunchApp(const std::wstring& cmd_line);
};

// Holds a queue of sent synchronous IPC messages. Knows how to dispatch
// the replies.
class SyncMsgSender {
 public:
  explicit SyncMsgSender(TabsMap* tab2delegate);
  void QueueSyncMessage(const IPC::SyncMessage* msg,
                        ChromeProxyDelegate* delegate, SyncMessageContext* ctx);
  bool OnReplyReceived(const IPC::Message* reply_msg);
  void OnChannelClosed();
  void Cancel(ChromeProxyDelegate* delegate);
 private:
  // sync_message_id -> (message_type, delegate, context)
  struct SingleSentMessage {
    SingleSentMessage() : type_(0), ctx_(NULL), delegate_(NULL) {}
    SingleSentMessage(uint32 type,
                      ChromeProxyDelegate* delegate,
                      SyncMessageContext* ctx)
        : type_(type), ctx_(ctx), delegate_(delegate) {}
    ~SingleSentMessage() { delete ctx_; }
    uint32 type_;
    SyncMessageContext* ctx_;
    ChromeProxyDelegate* delegate_;
  };

  SingleSentMessage* RemoveMessage(int id);
  typedef std::map<int, SingleSentMessage*> SentMessages;
  SentMessages messages_;
  base::Lock messages_lock_;
  TabsMap* tab2delegate_;
};

// Converts method call to an IPC message and then send it over Message::Sender
class Interface2IPCMessage : public ChromeProxy {
 public:
  Interface2IPCMessage() {}

  // General
  virtual void RemoveBrowsingData(int mask);
  virtual void SetProxyConfig(const std::string& json_encoded_proxy_cfg);
  // Tab related.
  virtual void Tab_PostMessage(int tab, const std::string& message,
      const std::string& origin, const std::string& target);
  virtual void Tab_Reload(int tab);
  virtual void Tab_Stop(int tab);
  virtual void Tab_SaveAs(int tab);
  virtual void Tab_Print(int tab);
  virtual void Tab_Cut(int tab);
  virtual void Tab_Copy(int tab);
  virtual void Tab_Paste(int tab);
  virtual void Tab_SelectAll(int tab);
  virtual void Tab_MenuCommand(int tab, int selected_command);
  virtual void Tab_Zoom(int tab, PageZoom::Function zoom_level);
  virtual void Tab_FontSize(int tab, enum AutomationPageFontSize font_size);
  virtual void Tab_SetInitialFocus(int tab, bool reverse,
                                   bool restore_focus_to_view);
  virtual void Tab_SetParentWindow(int tab);
  virtual void Tab_Resize(int tab);
  virtual void Tab_ProcessAccelerator(int tab, const MSG& msg);

  // Misc.
  virtual void Tab_OnHostMoved(int tab);
  virtual void Tab_SetEnableExtensionAutomation(int tab,
      const std::vector<std::string>& functions_enabled);
 protected:
  ~Interface2IPCMessage() {}
 private:
  IPC::Message::Sender* sender_;
};

// Simple class to keep a list of pointers to ChromeProxyDelegate for a
// specific proxy as well as mapping between tab_id -> ChromeProxyDelegate.
class DelegateHolder {
 protected:
  DelegateHolder() {
  }

  void AddDelegate(ChromeProxyDelegate* p);
  void RemoveDelegate(ChromeProxyDelegate* p);
  // Helper
  ChromeProxyDelegate* Tab2Delegate(int tab_handle);
  TabsMap tab2delegate_;
  typedef std::set<ChromeProxyDelegate*> DelegateList;
  DelegateList delegate_list_;
};

// ChromeFrame Automation Proxy implementation.
class CFProxy : public Interface2IPCMessage,
                public IPC::Channel::Listener,
                public DelegateHolder {
 public:
  explicit CFProxy(CFProxyTraits* api);
  ~CFProxy();

 private:
  virtual void Init(const ProxyParams& params);
  virtual int AddDelegate(ChromeProxyDelegate* p);
  virtual int RemoveDelegate(ChromeProxyDelegate* p);

  // Executed in IPC thread.
  void AddDelegateOnIoThread(ChromeProxyDelegate* p);
  void RemoveDelegateOnIoThread(ChromeProxyDelegate* p);
  // Initialization that has to be mede in IPC thread.
  void InitInIoThread(const ProxyParams& params);
  // Cleanup that has to be made in IPC thread.
  void CleanupOnIoThread();
  // IPC connection was not established in timely manner
  void LaunchTimeOut();
  // Close channel, inform delegates.
  void OnPeerLost(ChromeProxyDelegate::DisconnectReason reason);
  // Queues message to be send in IPC thread.
  void SendIpcMessage(IPC::Message* m);
  // Same but in IPC thread.
  void SendIpcMessageOnIoThread(IPC::Message* m);

  //////////////////////////////////////////////////////////////////////////
  // Sync messages.
  virtual void InstallExtension(ChromeProxyDelegate* delegate,
      const FilePath& crx_path, SyncMessageContext* ctx);
  virtual void LoadExtension(ChromeProxyDelegate* delegate,
                             const FilePath& path, SyncMessageContext* ctx);
  virtual void GetEnabledExtensions(ChromeProxyDelegate* delegate,
                                    SyncMessageContext* ctx);
  virtual void Tab_Find(int tab, const string16& search_string,
      FindInPageDirection forward, FindInPageCase match_case, bool find_next);
  virtual void Tab_OverrideEncoding(int tab, const char* encoding);
  virtual void Tab_Navigate(int tab, const GURL& url, const GURL& referrer);
  virtual void CreateTab(ChromeProxyDelegate* delegate,
                         const ExternalTabSettings& p);
  virtual void ConnectTab(ChromeProxyDelegate* delegate, HWND hwnd,
                          uint64 cookie);
  virtual void BlockTab(uint64 cookie);
  virtual void Tab_RunUnloadHandlers(int tab);

  //////////////////////////////////////////////////////////////////////////
  // IPC::Channel::Listener
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  bool CalledOnIpcThread() const {
    return base::PlatformThread::CurrentId() == ipc_thread_.thread_id();
  }

  base::Thread ipc_thread_;
  SyncMsgSender sync_dispatcher_;
  IPC::Message::Sender* ipc_sender_;
  CFProxyTraits* api_;
  int delegate_count_;
  bool is_connected_;
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(CFProxy);

// Support functions.
std::string GenerateChannelId();
std::wstring BuildCmdLine(const std::string& channel_id,
                          const FilePath& profile_path,
                          const std::wstring& extra_args);
#endif  // CHROME_FRAME_CFPROXY_PRIVATE_H_

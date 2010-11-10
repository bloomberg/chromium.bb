// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/cfproxy_private.h"

#include <vector>
#include "base/atomic_sequence_num.h"
#include "base/command_line.h"
#include "base/process_util.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/chrome_launcher_utils.h"
#include "chrome_frame/utils.h"  // for IsHeadlessMode();


namespace {
void DispatchReplyFail(uint32 type,
                       ChromeProxyDelegate* delegate,
                       SyncMessageContext* ctx) {
  switch (type) {
    case AutomationMsg_CreateExternalTab::ID:
      delegate->Completed_CreateTab(false, NULL, NULL, 0, 0);
      break;
    case AutomationMsg_ConnectExternalTab::ID:
      delegate->Completed_ConnectToTab(false, NULL, NULL, 0, 0);
      break;
    case AutomationMsg_InstallExtension::ID:
      delegate->Completed_InstallExtension(false,
          AUTOMATION_MSG_EXTENSION_INSTALL_FAILED, ctx);
      break;
  }
}

bool DispatchReplyOk(const IPC::Message* reply_msg, uint32 type,
                     ChromeProxyDelegate* delegate, SyncMessageContext* ctx,
                     TabsMap* tab2delegate) {
  void* iter = IPC::SyncMessage::GetDataIterator(reply_msg);
  switch (type) {
    case AutomationMsg_CreateExternalTab::ID: {
      // Tuple4<HWND, HWND, int, int> out;
      TupleTypes<AutomationMsg_CreateExternalTab::ReplyParam>::ValueTuple out;
      if (ReadParam(reply_msg, &iter, &out)) {
        DCHECK(tab2delegate->find(out.c) == tab2delegate->end());
        (*tab2delegate)[out.c] = delegate;
        delegate->Completed_CreateTab(true, out.a, out.b, out.c, out.d);
      }
      return true;
    }

    case AutomationMsg_ConnectExternalTab::ID: {
      // Tuple4<HWND, HWND, int, int> out;
      TupleTypes<AutomationMsg_ConnectExternalTab::ReplyParam>::ValueTuple out;
      if (ReadParam(reply_msg, &iter, &out)) {
        DCHECK(tab2delegate->find(out.c) == tab2delegate->end());
        (*tab2delegate)[out.c] = delegate;
        delegate->Completed_ConnectToTab(true, out.a, out.b, out.c, out.d);
      }
      return true;
    }

    case AutomationMsg_InstallExtension::ID: {
      // Tuple1<AutomationMsg_ExtensionResponseValues> out;
      TupleTypes<AutomationMsg_InstallExtension::ReplyParam>::ValueTuple out;
      if (ReadParam(reply_msg, &iter, &out))
        delegate->Completed_InstallExtension(true, out.a, ctx);
      return true;
    }

    case AutomationMsg_LoadExpandedExtension::ID: {
      // Tuple1<AutomationMsg_ExtensionResponseValues> out;
      TupleTypes<AutomationMsg_LoadExpandedExtension::ReplyParam>::ValueTuple
          out;
      if (ReadParam(reply_msg, &iter, &out))
        delegate->Completed_LoadExpandedExtension(true, out.a, ctx);
      break;
    }

    case AutomationMsg_GetEnabledExtensions::ID: {
      // Tuple1<std::vector<FilePath> >
      TupleTypes<AutomationMsg_GetEnabledExtensions::ReplyParam>::ValueTuple
          out;
      if (ReadParam(reply_msg, &iter, &out))
        delegate->Completed_GetEnabledExtensions(true, &out.a);
      break;
    }
  }  // switch

  return false;
}
}  // namespace

// Itf2IPCMessage
// Converts and sends trivial messages.
void Interface2IPCMessage::RemoveBrowsingData(int mask) {
  sender_->Send(new AutomationMsg_RemoveBrowsingData(0, mask));
}

void Interface2IPCMessage::SetProxyConfig(
    const std::string& json_encoded_proxy_cfg) {
  sender_->Send(new AutomationMsg_SetProxyConfig(0, json_encoded_proxy_cfg));
}

// Tab related.
void Interface2IPCMessage::Tab_PostMessage(int tab, const std::string& message,
      const std::string& origin, const std::string& target) {
  sender_->Send(new AutomationMsg_HandleMessageFromExternalHost(
      0, tab, message, origin, target));
}

void Interface2IPCMessage::Tab_Reload(int tab) {
  sender_->Send(new AutomationMsg_ReloadAsync(0, tab));
}

void Interface2IPCMessage::Tab_Stop(int tab) {
  sender_->Send(new AutomationMsg_StopAsync(0, tab));
}

void Interface2IPCMessage::Tab_SaveAs(int tab) {
  sender_->Send(new AutomationMsg_SaveAsAsync(0, tab));
}

void Interface2IPCMessage::Tab_Print(int tab) {
  sender_->Send(new AutomationMsg_PrintAsync(0, tab));
}

void Interface2IPCMessage::Tab_Cut(int tab) {
  sender_->Send(new AutomationMsg_Cut(0, tab));
}

void Interface2IPCMessage::Tab_Copy(int tab) {
  sender_->Send(new AutomationMsg_Copy(0, tab));
}

void Interface2IPCMessage::Tab_Paste(int tab) {
  sender_->Send(new AutomationMsg_Paste(0, tab));
}

void Interface2IPCMessage::Tab_SelectAll(int tab) {
  sender_->Send(new AutomationMsg_SelectAll(0, tab));
}

void Interface2IPCMessage::Tab_MenuCommand(int tab, int selected_command) {
  sender_->Send(new AutomationMsg_ForwardContextMenuCommandToChrome(
      0, tab, selected_command));
}

void Interface2IPCMessage::Tab_Zoom(int tab, PageZoom::Function zoom_level) {
  sender_->Send(new AutomationMsg_SetZoomLevel(0, tab, zoom_level));
}

void Interface2IPCMessage::Tab_FontSize(int tab,
                                        enum AutomationPageFontSize font_size) {
  sender_->Send(new AutomationMsg_SetPageFontSize(0, tab, font_size));
}

void Interface2IPCMessage::Tab_SetInitialFocus(int tab, bool reverse,
                                         bool restore_focus_to_view) {
  sender_->Send(new AutomationMsg_SetInitialFocus(0, tab, reverse,
                                                  restore_focus_to_view));
}

void Interface2IPCMessage::Tab_SetParentWindow(int tab) {
  CHECK(0) << "Implement me";
  // AutomationMsg_TabReposition
}

void Interface2IPCMessage::Tab_Resize(int tab) {
  CHECK(0) << "Implement me";
  // AutomationMsg_TabReposition
}

void Interface2IPCMessage::Tab_ProcessAccelerator(int tab, const MSG& msg) {
  sender_->Send(new AutomationMsg_ProcessUnhandledAccelerator(0, tab, msg));
}

// Misc.
void Interface2IPCMessage::Tab_OnHostMoved(int tab) {
  sender_->Send(new AutomationMsg_BrowserMove(0, tab));
}

void Interface2IPCMessage::Tab_SetEnableExtensionAutomation(int tab,
    const std::vector<std::string>& functions_enabled) {
  sender_->Send(new AutomationMsg_SetEnableExtensionAutomation(0, tab,
                functions_enabled));
}

void DelegateHolder::AddDelegate(ChromeProxyDelegate* p) {
  delegate_list_.insert(p);
}

void DelegateHolder::RemoveDelegate(ChromeProxyDelegate* p) {
  // DCHECK(CalledOnValidThread());
  int tab_handle = p->tab_handle();  // Could be 0.
  delegate_list_.erase(p);
  tab2delegate_.erase(tab_handle);
}

ChromeProxyDelegate* DelegateHolder::Tab2Delegate(int tab_handle) {
  TabsMap::const_iterator iter = tab2delegate_.find(tab_handle);
  if (iter != tab2delegate_.end())
    return iter->second;
  return NULL;
}

SyncMsgSender::SyncMsgSender(TabsMap* tab2delegate)
    : tab2delegate_(tab2delegate) {
}

// The outgoing queue of sync messages must be locked.
// Case: ui thread is sending message and waits for event, that is going to be
// signaled by completion handler in ipc_thread.
// We must append the message to the outgoing queue in UI thread,
// otherwise if channel is disconnected before having a chance to
// send the message, the ChromeProxyDelegate::_Disconnect implementation
// shall know how to unblock arbitrary sync call. Instead
// ChromeProxyDelgate::Completed_XXXX knows how to unblock a specific one.
void SyncMsgSender::QueueSyncMessage(const IPC::SyncMessage* msg,
                                     ChromeProxyDelegate* delegate,
                                     SyncMessageContext* ctx) {
  if (delegate) {
    // We are interested of the result.
    AutoLock lock(messages_lock_);
    int id = IPC::SyncMessage::GetMessageId(*msg);
    // A message can be sent only once.
    DCHECK(messages_.end() == messages_.find(id));
    messages_[id] = new SingleSentMessage(msg->type(), delegate, ctx);
  }
}

// Cancel all outgoing calls for this delegate.
void SyncMsgSender::Cancel(ChromeProxyDelegate* delegate) {
  std::vector<SingleSentMessage*> cancelled;
  {
    AutoLock lock(messages_lock_);
    SentMessages::iterator it = messages_.begin();
    for (; it != messages_.end(); ) {
      SingleSentMessage* origin = it->second;
      if (origin->delegate_ == delegate) {
        cancelled.push_back(origin);
        it = messages_.erase(it);
      } else {
        ++it;
      }
    }
  }

  for (std::vector<SingleSentMessage*>::iterator it = cancelled.begin();
       it != cancelled.end(); ++it) {
    SingleSentMessage* origin = *it;
    DispatchReplyFail(origin->type_, delegate, origin->ctx_);
    delete origin;
  }
}

SyncMsgSender::SingleSentMessage* SyncMsgSender::RemoveMessage(int id) {
  AutoLock lock(messages_lock_);
  SentMessages::iterator it = messages_.find(id);
  if (it == messages_.end()) {
    // Delegate is not interested in this sync message response.
    return NULL;
  }

  // See what message is this.
  SingleSentMessage* origin = it->second;
  messages_.erase(it);
  return origin;
}

bool SyncMsgSender::OnReplyReceived(const IPC::Message* reply_msg) {
  if (!reply_msg->is_reply())
    return false;  // Not a reply to sync message.

  // Find message by id.
  int id = IPC::SyncMessage::GetMessageId(*reply_msg);
  SingleSentMessage* origin = RemoveMessage(id);
  if (origin) {
    DispatchReplyOk(reply_msg, origin->type_, origin->delegate_, origin->ctx_,
                    tab2delegate_);
    delete origin;
  }

  return true;
}

void SyncMsgSender::OnChannelClosed() {
  SentMessages messages_sent;
  // Make a copy of the messages queue
  {
    AutoLock lock(messages_lock_);
    messages_.swap(messages_sent);
  }


  SentMessages::reverse_iterator it = messages_sent.rbegin();
  for (; it != messages_sent.rend(); ++it) {
    SingleSentMessage* origin = it->second;
    DispatchReplyFail(origin->type_, origin->delegate_, origin->ctx_);
    delete origin;
  }
  messages_sent.clear();
}

static base::AtomicSequenceNumber g_proxy_channel_id(base::LINKER_INITIALIZED);
std::string GenerateChannelId() {
  return StringPrintf("ChromeTestingInterface:%u.%d",
      base::GetCurrentProcId(), g_proxy_channel_id.GetNext() + 0xC000);
}

std::wstring BuildCmdLine(const std::string& channel_id,
                          const FilePath& profile_path,
                          const std::wstring& extra_args) {
  scoped_ptr<CommandLine> command_line(
      chrome_launcher::CreateLaunchCommandLine());
  command_line->AppendSwitchASCII(switches::kAutomationClientChannelID,
      channel_id);
  // Run Chrome in Chrome Frame mode. In practice, this modifies the paths
  // and registry keys that Chrome looks in via the BrowserDistribution
  // mechanism.
  command_line->AppendSwitch(switches::kChromeFrame);
  // Chrome Frame never wants Chrome to start up with a First Run UI.
  command_line->AppendSwitch(switches::kNoFirstRun);
  command_line->AppendSwitch(switches::kDisablePopupBlocking);

#ifndef NDEBUG
  // Disable the "Whoa! Chrome has crashed." dialog, because that isn't very
  // useful for Chrome Frame users.
  command_line->AppendSwitch(switches::kNoErrorDialogs);
#endif

  // In headless mode runs like reliability test runs we want full crash dumps
  // from chrome.
  if (IsHeadlessMode())
    command_line->AppendSwitch(switches::kFullMemoryCrashReport);

  command_line->AppendSwitchPath(switches::kUserDataDir, profile_path);

  std::wstring command_line_string(command_line->command_line_string());
  if (!extra_args.empty()) {
    command_line_string.append(L" ");
    command_line_string.append(extra_args);
  }
  return command_line_string;
}

int IsTabMessage(const IPC::Message& message) {
  switch (message.type()) {
    case AutomationMsg_NavigationStateChanged__ID:
    case AutomationMsg_UpdateTargetUrl__ID:
    case AutomationMsg_HandleAccelerator__ID:
    case AutomationMsg_TabbedOut__ID:
    case AutomationMsg_OpenURL__ID:
    case AutomationMsg_NavigationFailed__ID:
    case AutomationMsg_DidNavigate__ID:
    case AutomationMsg_TabLoaded__ID:
    case AutomationMsg_ForwardMessageToExternalHost__ID:
    case AutomationMsg_ForwardContextMenuToExternalHost__ID:
    case AutomationMsg_RequestStart__ID:
    case AutomationMsg_RequestRead__ID:
    case AutomationMsg_RequestEnd__ID:
    case AutomationMsg_DownloadRequestInHost__ID:
    case AutomationMsg_SetCookieAsync__ID:
    case AutomationMsg_AttachExternalTab__ID:
    case AutomationMsg_RequestGoToHistoryEntryOffset__ID:
    case AutomationMsg_GetCookiesFromHost__ID:
    case AutomationMsg_CloseExternalTab__ID: {
      // Read tab handle from the message.
      void* iter = NULL;
      int tab_handle = 0;
      message.ReadInt(&iter, &tab_handle);
      return tab_handle;
    }
  }

  return 0;
}

bool DispatchTabMessageToDelegate(ChromeProxyDelegate* delegate,
                                  const IPC::Message& m) {
  // The first argument of the message is always the tab handle.
  void* iter = 0;
  switch (m.type()) {
    case AutomationMsg_NavigationStateChanged__ID: {
      // Tuple3<int, int, IPC::NavigationInfo>
      AutomationMsg_NavigationStateChanged::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->NavigationStateChanged(params.b, params.c);
      return true;
    }

    case AutomationMsg_UpdateTargetUrl__ID: {
      // Tuple2<int, std::wstring>
      AutomationMsg_UpdateTargetUrl::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->UpdateTargetUrl(params.b);
      return true;
    }

    case AutomationMsg_HandleAccelerator__ID: {
      // Tuple2<int, MSG>
      AutomationMsg_HandleAccelerator::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->HandleAccelerator(params.b);
      return true;
    }

    case AutomationMsg_TabbedOut__ID: {
      // Tuple2<int, bool>
      AutomationMsg_TabbedOut::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->TabbedOut(params.b);
      return true;
    }

    case AutomationMsg_OpenURL__ID: {
      // Tuple4<int, GURL, GURL, int>
      AutomationMsg_OpenURL::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->OpenURL(params.b, params.c, params.d);
      return true;
    }

    case AutomationMsg_NavigationFailed__ID: {
      // Tuple3<int, int, GURL>
      AutomationMsg_NavigationFailed::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->NavigationFailed(params.b, params.c);
      return true;
    }

    case AutomationMsg_DidNavigate__ID: {
      // Tuple2<int, IPC::NavigationInfo>
      AutomationMsg_DidNavigate::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->DidNavigate(params.b);
      return true;
    }

    case AutomationMsg_TabLoaded__ID: {
      // Tuple2<int, GURL>
      AutomationMsg_TabLoaded::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->TabLoaded(params.b);
      return true;
    }

    case AutomationMsg_ForwardMessageToExternalHost__ID: {
      // Tuple4<int, string, string, string>
      AutomationMsg_ForwardMessageToExternalHost::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->MessageToHost(params.b, params.c, params.d);
      return true;
    }

    case AutomationMsg_ForwardContextMenuToExternalHost__ID: {
      // Tuple4<int, HANDLE, int, IPC::ContextMenuParams>
      AutomationMsg_ForwardContextMenuToExternalHost::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->HandleContextMenu(params.b, params.c, params.d);
      return true;
    }

    case AutomationMsg_RequestStart__ID: {
      // Tuple3<int, int, IPC::AutomationURLRequest>
      AutomationMsg_RequestStart::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->Network_Start(params.b, params.c);
      return true;
    }

    case AutomationMsg_RequestRead__ID: {
      // Tuple3<int, int, int>
      AutomationMsg_RequestRead::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->Network_Read(params.b, params.c);
      return true;
    }

    case AutomationMsg_RequestEnd__ID: {
      // Tuple3<int, int, URLRequestStatus>
      AutomationMsg_RequestEnd::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->Network_End(params.b, params.c);
      return true;
    }

    case AutomationMsg_DownloadRequestInHost__ID: {
      // Tuple2<int, int>
      AutomationMsg_DownloadRequestInHost::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->Network_DownloadInHost(params.b);
      return true;
    }

    case AutomationMsg_SetCookieAsync__ID: {
      // Tuple3<int, GURL, string>
      AutomationMsg_SetCookieAsync::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->SetCookie(params.b, params.c);
      return true;
    }

    case AutomationMsg_AttachExternalTab__ID: {
      // Tuple2<int, IPC::AttachExternalTabParams>
      AutomationMsg_AttachExternalTab::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->AttachTab(params.b);
      return true;
    }

    case AutomationMsg_RequestGoToHistoryEntryOffset__ID: {
      // Tuple2<int, int>
      AutomationMsg_RequestGoToHistoryEntryOffset::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->GoToHistoryOffset(params.b);
      return true;
    }

    case AutomationMsg_GetCookiesFromHost__ID: {
      // Tuple3<int, GURL, int>
      AutomationMsg_GetCookiesFromHost::Param params;
      if (ReadParam(&m, &iter, &params))
        delegate->GetCookies(params.b, params.c);
      return true;
    }

    case AutomationMsg_CloseExternalTab__ID: {
      // Tuple1<int>
      delegate->TabClosed();
      return true;
    }
  }

  return false;
}

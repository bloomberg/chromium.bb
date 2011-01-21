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
  sender_->Send(new AutomationMsg_RemoveBrowsingData(mask));
}

void Interface2IPCMessage::SetProxyConfig(
    const std::string& json_encoded_proxy_cfg) {
  sender_->Send(new AutomationMsg_SetProxyConfig(json_encoded_proxy_cfg));
}

// Tab related.
void Interface2IPCMessage::Tab_PostMessage(int tab, const std::string& message,
      const std::string& origin, const std::string& target) {
  sender_->Send(new AutomationMsg_HandleMessageFromExternalHost(
      tab, message, origin, target));
}

void Interface2IPCMessage::Tab_Reload(int tab) {
  sender_->Send(new AutomationMsg_ReloadAsync(tab));
}

void Interface2IPCMessage::Tab_Stop(int tab) {
  sender_->Send(new AutomationMsg_StopAsync(tab));
}

void Interface2IPCMessage::Tab_SaveAs(int tab) {
  sender_->Send(new AutomationMsg_SaveAsAsync(tab));
}

void Interface2IPCMessage::Tab_Print(int tab) {
  sender_->Send(new AutomationMsg_PrintAsync(tab));
}

void Interface2IPCMessage::Tab_Cut(int tab) {
  sender_->Send(new AutomationMsg_Cut(tab));
}

void Interface2IPCMessage::Tab_Copy(int tab) {
  sender_->Send(new AutomationMsg_Copy(tab));
}

void Interface2IPCMessage::Tab_Paste(int tab) {
  sender_->Send(new AutomationMsg_Paste(tab));
}

void Interface2IPCMessage::Tab_SelectAll(int tab) {
  sender_->Send(new AutomationMsg_SelectAll(tab));
}

void Interface2IPCMessage::Tab_MenuCommand(int tab, int selected_command) {
  sender_->Send(new AutomationMsg_ForwardContextMenuCommandToChrome(
      tab, selected_command));
}

void Interface2IPCMessage::Tab_Zoom(int tab, PageZoom::Function zoom_level) {
  sender_->Send(new AutomationMsg_SetZoomLevel(tab, zoom_level));
}

void Interface2IPCMessage::Tab_FontSize(int tab,
                                        enum AutomationPageFontSize font_size) {
  sender_->Send(new AutomationMsg_SetPageFontSize(tab, font_size));
}

void Interface2IPCMessage::Tab_SetInitialFocus(int tab, bool reverse,
                                         bool restore_focus_to_view) {
  sender_->Send(new AutomationMsg_SetInitialFocus(tab, reverse,
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
  sender_->Send(new AutomationMsg_ProcessUnhandledAccelerator(tab, msg));
}

// Misc.
void Interface2IPCMessage::Tab_OnHostMoved(int tab) {
  sender_->Send(new AutomationMsg_BrowserMove(tab));
}

void Interface2IPCMessage::Tab_SetEnableExtensionAutomation(int tab,
    const std::vector<std::string>& functions_enabled) {
  sender_->Send(new AutomationMsg_SetEnableExtensionAutomation(tab,
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
    base::AutoLock lock(messages_lock_);
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
    base::AutoLock lock(messages_lock_);
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
  base::AutoLock lock(messages_lock_);
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
    base::AutoLock lock(messages_lock_);
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

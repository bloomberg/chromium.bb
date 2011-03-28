// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Debugger API.

#include "chrome/browser/extensions/extension_debugger_api.h"

#include <set>

#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/extensions/extension_debugger_api_constants.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"

namespace keys = extension_debugger_api_constants;

class ExtensionDevToolsClientHost : public DevToolsClientHost,
                                    public NotificationObserver {
 public:
  ExtensionDevToolsClientHost(TabContents* tab_contents,
                              const std::string& extension_id,
                              int tab_id);

  ~ExtensionDevToolsClientHost();

  bool MatchesContentsAndExtensionId(TabContents* tab_contents,
                                     const std::string& extension_id);
  void Close();
  void SendMessageToBackend(const std::string& message);

  // DevToolsClientHost interface
  virtual void InspectedTabClosing();
  virtual void SendMessageToClient(const IPC::Message& msg);
  virtual void TabReplaced(TabContentsWrapper* tab_contents);
  virtual void FrameNavigating(const std::string& url) {}

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
  void OnDispatchOnInspectorFrontend(const std::string& data);

  TabContents* tab_contents_;
  std::string extension_id_;
  int tab_id_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsClientHost);
};

namespace {

class AttachedClientHosts {
 public:
  AttachedClientHosts() {}

  // Returns the singleton instance of this class
  static AttachedClientHosts* GetInstance() {
    return Singleton<AttachedClientHosts>::get();
  }

  void Add(ExtensionDevToolsClientHost* client_host) {
    client_hosts_.insert(client_host);
  }

  void Remove(ExtensionDevToolsClientHost* client_host) {
    client_hosts_.erase(client_host);
  }

  ExtensionDevToolsClientHost* Lookup(RenderViewHost* rvh) {
    DevToolsClientHost* client_host =
        DevToolsManager::GetInstance()->GetDevToolsClientHostFor(rvh);
    std::set<DevToolsClientHost*>::iterator it =
        client_hosts_.find(client_host);
    if (it == client_hosts_.end())
      return NULL;
    return static_cast<ExtensionDevToolsClientHost*>(client_host);
  }

 private:
  std::set<DevToolsClientHost*> client_hosts_;
};

}  // namespace

ExtensionDevToolsClientHost::ExtensionDevToolsClientHost(
    TabContents* tab_contents,
    const std::string& extension_id,
    int tab_id)
    : tab_contents_(tab_contents),
      extension_id_(extension_id),
      tab_id_(tab_id) {
  AttachedClientHosts::GetInstance()->Add(this);

  // Detach from debugger when extension unloads.
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(tab_contents_->profile()));

  // Attach to debugger and tell it we are ready.
  DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
      tab_contents_->render_view_host(),
      this);
  DevToolsManager::GetInstance()->ForwardToDevToolsAgent(
      this,
      DevToolsAgentMsg_FrontendLoaded());
}

ExtensionDevToolsClientHost::~ExtensionDevToolsClientHost() {
  AttachedClientHosts::GetInstance()->Remove(this);
}

bool ExtensionDevToolsClientHost::MatchesContentsAndExtensionId(
    TabContents* tab_contents,
    const std::string& extension_id) {
  return tab_contents == tab_contents_ && extension_id_ == extension_id;
}

// DevToolsClientHost interface
void ExtensionDevToolsClientHost::InspectedTabClosing() {
  // Tell extension that this client host has been detached.
  Profile* profile = tab_contents_->profile();
  if (profile != NULL && profile->GetExtensionEventRouter()) {
    ListValue args;
    args.Append(Value::CreateIntegerValue(tab_id_));

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);

    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, keys::kOnDetach, json_args, profile, GURL());
  }
  delete this;
}

void ExtensionDevToolsClientHost::SendMessageToClient(
    const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(ExtensionDevToolsClientHost, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend);
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void ExtensionDevToolsClientHost::TabReplaced(
    TabContentsWrapper* tab_contents) {
  tab_contents_ = tab_contents->tab_contents();
}

void ExtensionDevToolsClientHost::Close() {
  DevToolsClientHost::NotifyCloseListener();
  delete this;
}

void ExtensionDevToolsClientHost::SendMessageToBackend(
    const std::string& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsAgent(
      this,
      DevToolsAgentMsg_DispatchOnInspectorBackend(message));
}

void ExtensionDevToolsClientHost::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::EXTENSION_UNLOADED);
  Close();
}

void ExtensionDevToolsClientHost::OnDispatchOnInspectorFrontend(
    const std::string& data) {
  Profile* profile = tab_contents_->profile();
  if (profile != NULL && profile->GetExtensionEventRouter()) {
    // data is stringified JSON object, while extension expects
    // array of objects (tabId, data).
    std::string message = StringPrintf("[%d,%s]", tab_id_, data.c_str());

    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, keys::kOnMessage, message, profile, GURL());
  }
}

DebuggerFunction::DebuggerFunction()
    : contents_(0),
      client_host_(0) {
}

bool DebuggerFunction::InitTabContents(int tab_id) {
  // Find the TabContents that contains this tab id.
  contents_ = NULL;
  TabContentsWrapper* wrapper = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id, profile(), include_incognito(), NULL, NULL, &wrapper, NULL);
  if (!result || !wrapper) {
    error_ = error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoTabError,
        base::IntToString(tab_id));
    return false;
  }
  contents_ = wrapper->tab_contents();
  return true;
}

bool DebuggerFunction::InitClientHost(int tab_id) {
  if (!InitTabContents(tab_id))
    return false;

  RenderViewHost* rvh = contents_->render_view_host();
  client_host_ = AttachedClientHosts::GetInstance()->Lookup(rvh);

  if (!client_host_ ||
      !client_host_->MatchesContentsAndExtensionId(contents_,
                                                   GetExtension()->id())) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNotAttachedError,
        base::IntToString(tab_id));
    return false;
  }
  return true;
}

AttachDebuggerFunction::AttachDebuggerFunction() {}

AttachDebuggerFunction::~AttachDebuggerFunction() {}

bool AttachDebuggerFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  if (!InitTabContents(tab_id))
    return false;

  DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
      GetDevToolsClientHostFor(contents_->render_view_host());

  if (client_host != NULL) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kAlreadyAttachedError,
        base::IntToString(tab_id));
    return false;
  }

  new ExtensionDevToolsClientHost(contents_, GetExtension()->id(), tab_id);
  return true;
}

DetachDebuggerFunction::DetachDebuggerFunction() {}

DetachDebuggerFunction::~DetachDebuggerFunction() {}

bool DetachDebuggerFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  if (!InitClientHost(tab_id))
    return false;

  client_host_->Close();
  return true;
}

PostMessageDebuggerFunction::PostMessageDebuggerFunction() {}

PostMessageDebuggerFunction::~PostMessageDebuggerFunction() {}

bool PostMessageDebuggerFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  Value *command;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(1, &command));

  if (!InitClientHost(tab_id))
    return false;

  std::string json_args;
  base::JSONWriter::Write(command, false, &json_args);
  client_host_->SendMessageToBackend(json_args);
  return true;
}

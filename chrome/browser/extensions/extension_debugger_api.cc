// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Debugger API.

#include "chrome/browser/extensions/extension_debugger_api.h"

#include <map>
#include <set>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_debugger_api_constants.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_source.h"
#include "webkit/glue/webkit_glue.h"

using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsClientHost;
using content::DevToolsManager;

namespace keys = extension_debugger_api_constants;

class ExtensionDevToolsClientHost : public DevToolsClientHost,
                                    public content::NotificationObserver {
 public:
  ExtensionDevToolsClientHost(TabContents* tab_contents,
                              const std::string& extension_id,
                              int tab_id);

  ~ExtensionDevToolsClientHost();

  bool MatchesContentsAndExtensionId(TabContents* tab_contents,
                                     const std::string& extension_id);
  void Close();
  void SendMessageToBackend(SendCommandDebuggerFunction* function,
                            const std::string& method,
                            Value* params);

  // DevToolsClientHost interface
  virtual void InspectedTabClosing();
  virtual void DispatchOnInspectorFrontend(const std::string& message);
  virtual void TabReplaced(TabContents* tab_contents);
  virtual void FrameNavigating(const std::string& url) {}

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  TabContents* tab_contents_;
  std::string extension_id_;
  int tab_id_;
  content::NotificationRegistrar registrar_;
  int last_request_id_;
  typedef std::map<int, scoped_refptr<SendCommandDebuggerFunction> >
      PendingRequests;
  PendingRequests pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsClientHost);
};

namespace {

static Value* CreateDebuggeeId(int tab_id) {
  DictionaryValue* debuggeeId = new DictionaryValue();
  debuggeeId->SetInteger(keys::kTabIdKey, tab_id);
  return debuggeeId;
}

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
    if (!DevToolsAgentHostRegistry::HasDevToolsAgentHost(rvh))
      return NULL;
    DevToolsAgentHost* agent =
        DevToolsAgentHostRegistry::GetDevToolsAgentHost(rvh);
    DevToolsClientHost* client_host =
        DevToolsManager::GetInstance()->GetDevToolsClientHostFor(agent);
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
      tab_id_(tab_id),
      last_request_id_(0) {
  AttachedClientHosts::GetInstance()->Add(this);

  // Detach from debugger when extension unloads.
  Profile* profile =
      Profile::FromBrowserContext(tab_contents_->browser_context());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));

  // Attach to debugger and tell it we are ready.
  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      tab_contents_->render_view_host());
  DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(agent, this);
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
  Profile* profile =
      Profile::FromBrowserContext(tab_contents_->browser_context());
  if (profile != NULL && profile->GetExtensionEventRouter()) {
    ListValue args;
    args.Append(CreateDebuggeeId(tab_id_));

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);

    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, keys::kOnDetach, json_args, profile, GURL());
  }
  delete this;
}

void ExtensionDevToolsClientHost::TabReplaced(
    TabContents* tab_contents) {
  tab_contents_ = tab_contents;
}

void ExtensionDevToolsClientHost::Close() {
  DevToolsManager::GetInstance()->ClientHostClosing(this);
  delete this;
}

void ExtensionDevToolsClientHost::SendMessageToBackend(
    SendCommandDebuggerFunction* function,
    const std::string& method,
    Value* params) {
  DictionaryValue protocol_request;
  int request_id = ++last_request_id_;
  pending_requests_[request_id] = function;
  protocol_request.SetInteger("id", request_id);
  protocol_request.SetString("method", method);
  if (params)
    protocol_request.Set("params", params->DeepCopy());

  std::string json_args;
  base::JSONWriter::Write(&protocol_request, false, &json_args);
  DevToolsManager::GetInstance()->DispatchOnInspectorBackend(this, json_args);
}

void ExtensionDevToolsClientHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_UNLOADED);
  Close();
}

void ExtensionDevToolsClientHost::DispatchOnInspectorFrontend(
    const std::string& message) {
  Profile* profile =
      Profile::FromBrowserContext(tab_contents_->browser_context());
  if (profile == NULL || !profile->GetExtensionEventRouter())
    return;

  scoped_ptr<Value> result(base::JSONReader::Read(message, false));
  if (!result->IsType(Value::TYPE_DICTIONARY))
    return;
  DictionaryValue* dictionary = static_cast<DictionaryValue*>(result.get());

  int id;
  if (!dictionary->GetInteger("id", &id)) {
    std::string method_name;
    if (!dictionary->GetString("method", &method_name))
      return;

    ListValue args;
    args.Append(CreateDebuggeeId(tab_id_));
    args.Append(Value::CreateStringValue(method_name));
    Value* params_value;
    if (dictionary->Get("params", &params_value))
      args.Append(params_value->DeepCopy());

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);

    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, keys::kOnEvent, json_args, profile, GURL());
  } else {
    SendCommandDebuggerFunction* function = pending_requests_[id];
    if (!function)
      return;

    function->SendResponseBody(dictionary);
    pending_requests_.erase(id);
  }
}

DebuggerFunction::DebuggerFunction()
    : contents_(0),
      tab_id_(0),
      client_host_(0) {
}

bool DebuggerFunction::InitTabContents() {
  Value* debuggee;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &debuggee));

  DictionaryValue* dict = static_cast<DictionaryValue*>(debuggee);
  EXTENSION_FUNCTION_VALIDATE(dict->GetInteger(keys::kTabIdKey, &tab_id_));

  // Find the TabContents that contains this tab id.
  contents_ = NULL;
  TabContentsWrapper* wrapper = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id_, profile(), include_incognito(), NULL, NULL, &wrapper, NULL);
  if (!result || !wrapper) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoTabError,
        base::IntToString(tab_id_));
    return false;
  }
  contents_ = wrapper->tab_contents();
  return true;
}

bool DebuggerFunction::InitClientHost() {
  if (!InitTabContents())
    return false;

  RenderViewHost* rvh = contents_->render_view_host();
  client_host_ = AttachedClientHosts::GetInstance()->Lookup(rvh);

  if (!client_host_ ||
      !client_host_->MatchesContentsAndExtensionId(contents_,
                                                   GetExtension()->id())) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNotAttachedError,
        base::IntToString(tab_id_));
    return false;
  }
  return true;
}

AttachDebuggerFunction::AttachDebuggerFunction() {}

AttachDebuggerFunction::~AttachDebuggerFunction() {}

bool AttachDebuggerFunction::RunImpl() {
  if (!InitTabContents())
    return false;

  std::string version;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &version));

  if (!webkit_glue::IsInspectorProtocolVersionSupported(version)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kProtocolVersionNotSupportedError,
        version);
    return false;
  }

  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      contents_->render_view_host());
  DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
      GetDevToolsClientHostFor(agent);

  if (client_host != NULL) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kAlreadyAttachedError,
        base::IntToString(tab_id_));
    return false;
  }

  new ExtensionDevToolsClientHost(contents_, GetExtension()->id(), tab_id_);
  SendResponse(true);
  return true;
}

DetachDebuggerFunction::DetachDebuggerFunction() {}

DetachDebuggerFunction::~DetachDebuggerFunction() {}

bool DetachDebuggerFunction::RunImpl() {
  if (!InitClientHost())
    return false;

  client_host_->Close();
  SendResponse(true);
  return true;
}

SendCommandDebuggerFunction::SendCommandDebuggerFunction() {}

SendCommandDebuggerFunction::~SendCommandDebuggerFunction() {}

bool SendCommandDebuggerFunction::RunImpl() {

  if (!InitClientHost())
    return false;

  std::string method;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &method));

  Value *params;
  if (!args_->Get(2, &params))
    params = NULL;

  client_host_->SendMessageToBackend(this, method, params);
  return true;
}

void SendCommandDebuggerFunction::SendResponseBody(
    DictionaryValue* dictionary) {
  Value* error_body;
  if (dictionary->Get("error", &error_body)) {
    base::JSONWriter::Write(error_body, false, &error_);
    SendResponse(false);
    return;
  }

  Value* result_body;
  if (dictionary->Get("result", &result_body))
    result_.reset(result_body->DeepCopy());
  else
    result_.reset(new DictionaryValue());
  SendResponse(true);
}

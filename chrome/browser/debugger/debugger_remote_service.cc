// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains implementations of the DebuggerRemoteService methods,
// defines DebuggerRemoteService and DebuggerRemoteServiceCommand constants.

#include "chrome/browser/debugger/debugger_remote_service.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote_message.h"
#include "chrome/browser/debugger/inspectable_tab_proxy.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

namespace {

// Constants for the "data", "result", and "command" JSON message fields.
const char kDataKey[] = "data";
const char kResultKey[] = "result";
const char kCommandKey[] = "command";

}  // namespace

const std::string DebuggerRemoteServiceCommand::kAttach = "attach";
const std::string DebuggerRemoteServiceCommand::kDetach = "detach";
const std::string DebuggerRemoteServiceCommand::kDebuggerCommand =
    "debugger_command";
const std::string DebuggerRemoteServiceCommand::kEvaluateJavascript =
    "evaluate_javascript";
const std::string DebuggerRemoteServiceCommand::kFrameNavigate =
    "navigated";
const std::string DebuggerRemoteServiceCommand::kTabClosed =
    "closed";

const std::string DebuggerRemoteService::kToolName = "V8Debugger";

DebuggerRemoteService::DebuggerRemoteService(DevToolsProtocolHandler* delegate)
    : delegate_(delegate) {}

DebuggerRemoteService::~DebuggerRemoteService() {}

// This method handles the V8Debugger tool commands which are
// retrieved from the request "command" field. If an operation result
// is ready off-hand (synchronously), it is sent back to the remote debugger.
// Otherwise the corresponding response is received through IPC from the
// V8 debugger via DevToolsClientHost.
void DebuggerRemoteService::HandleMessage(
    const DevToolsRemoteMessage& message) {
  const std::string destination = message.destination();
  scoped_ptr<Value> request(base::JSONReader::Read(message.content(), true));
  if (request.get() == NULL) {
    // Bad JSON
    NOTREACHED();
    return;
  }
  DictionaryValue* content;
  if (!request->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED();  // Broken protocol :(
    return;
  }
  content = static_cast<DictionaryValue*>(request.get());
  if (!content->HasKey(kCommandKey)) {
    NOTREACHED();  // Broken protocol :(
    return;
  }
  std::string command;
  DictionaryValue response;

  content->GetString(kCommandKey, &command);
  response.SetString(kCommandKey, command);
  bool send_response = true;
  if (destination.size() == 0) {
    // Unknown command (bad format?)
    NOTREACHED();
    response.SetInteger(kResultKey, RESULT_UNKNOWN_COMMAND);
    SendResponse(response, message.tool(), message.destination());
    return;
  }
  int32 tab_uid = -1;
  base::StringToInt(destination, &tab_uid);

  if (command == DebuggerRemoteServiceCommand::kAttach) {
    // TODO(apavlov): handle 0 for a new tab
    response.SetString(kCommandKey, DebuggerRemoteServiceCommand::kAttach);
    AttachToTab(destination, &response);
  } else if (command == DebuggerRemoteServiceCommand::kDetach) {
    response.SetString(kCommandKey, DebuggerRemoteServiceCommand::kDetach);
    DetachFromTab(destination, &response);
  } else if (command == DebuggerRemoteServiceCommand::kDebuggerCommand) {
    send_response = DispatchDebuggerCommand(tab_uid, content, &response);
  } else if (command == DebuggerRemoteServiceCommand::kEvaluateJavascript) {
    send_response = DispatchEvaluateJavascript(tab_uid, content, &response);
  } else {
    // Unknown command
    NOTREACHED();
    response.SetInteger(kResultKey, RESULT_UNKNOWN_COMMAND);
  }

  if (send_response) {
    SendResponse(response, message.tool(), message.destination());
  }
}

void DebuggerRemoteService::OnConnectionLost() {
  delegate_->inspectable_tab_proxy()->OnRemoteDebuggerDetached();
}

// Sends a JSON response to the remote debugger using |response| as content,
// |tool| and |destination| as the respective header values.
void DebuggerRemoteService::SendResponse(const Value& response,
                                         const std::string& tool,
                                         const std::string& destination) {
  std::string response_content;
  base::JSONWriter::Write(&response, false, &response_content);
  scoped_ptr<DevToolsRemoteMessage> response_message(
      DevToolsRemoteMessageBuilder::instance().Create(tool,
                                                      destination,
                                                      response_content));
  delegate_->Send(*response_message.get());
}

// Gets a TabContents instance corresponding to the |tab_uid| using the
// InspectableTabProxy controllers map, or NULL if none found.
TabContents* DebuggerRemoteService::ToTabContents(int32 tab_uid) {
  const InspectableTabProxy::ControllersMap& navcon_map =
      delegate_->inspectable_tab_proxy()->controllers_map();
  InspectableTabProxy::ControllersMap::const_iterator it =
      navcon_map.find(tab_uid);
  if (it != navcon_map.end()) {
    TabContents* tab_contents = it->second->tab_contents();
    if (tab_contents == NULL) {
      return NULL;
    } else {
      return tab_contents;
    }
  } else {
    return NULL;
  }
}

// Gets invoked from a DevToolsClientHost callback whenever
// a message from the V8 VM debugger corresponding to |tab_id| is received.
// Composes a Chrome Developer Tools Protocol JSON response and sends it
// to the remote debugger.
void DebuggerRemoteService::DebuggerOutput(int32 tab_uid,
                                           const std::string& message) {
  std::string content = StringPrintf(
      "{\"command\":\"%s\",\"result\":%s,\"data\":%s}",
      DebuggerRemoteServiceCommand::kDebuggerCommand.c_str(),
      base::IntToString(RESULT_OK).c_str(),
      message.c_str());
  scoped_ptr<DevToolsRemoteMessage> response_message(
      DevToolsRemoteMessageBuilder::instance().Create(
          kToolName,
          base::IntToString(tab_uid),
          content));
  delegate_->Send(*(response_message.get()));
}

// Gets invoked from a DevToolsClientHost callback whenever
// a tab corresponding to |tab_id| changes its URL. |url| is the new
// URL of the tab (may be the same as the previous one if the tab is reloaded).
// Sends the corresponding message to the remote debugger.
void DebuggerRemoteService::FrameNavigate(int32 tab_uid,
                                          const std::string& url) {
  DictionaryValue value;
  value.SetString(kCommandKey, DebuggerRemoteServiceCommand::kFrameNavigate);
  value.SetInteger(kResultKey, RESULT_OK);
  value.SetString(kDataKey, url);
  SendResponse(value, kToolName, base::IntToString(tab_uid));
}

// Gets invoked from a DevToolsClientHost callback whenever
// a tab corresponding to |tab_id| gets closed.
// Sends the corresponding message to the remote debugger.
void DebuggerRemoteService::TabClosed(int32 tab_id) {
  DictionaryValue value;
  value.SetString(kCommandKey, DebuggerRemoteServiceCommand::kTabClosed);
  value.SetInteger(kResultKey, RESULT_OK);
  SendResponse(value, kToolName, base::IntToString(tab_id));
}

// Attaches a remote debugger to the target tab specified by |destination|
// by posting the DevToolsAgentMsg_Attach message and sends a response
// to the remote debugger immediately.
void DebuggerRemoteService::AttachToTab(const std::string& destination,
                                        DictionaryValue* response) {
  int32 tab_uid = -1;
  base::StringToInt(destination, &tab_uid);
  if (tab_uid < 0) {
    // Bad tab_uid received from remote debugger (perhaps NaN)
    response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    return;
  }
  if (tab_uid == 0) {  // single tab_uid
    // We've been asked to open a new tab with URL
    // TODO(apavlov): implement
    NOTIMPLEMENTED();
    response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    return;
  }
  TabContents* tab_contents = ToTabContents(tab_uid);
  if (tab_contents == NULL) {
    // No active tab contents with tab_uid
    response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    return;
  }
  RenderViewHost* target_host = tab_contents->render_view_host();
  DevToolsClientHost* client_host =
      delegate_->inspectable_tab_proxy()->ClientHostForTabId(tab_uid);
  if (client_host == NULL) {
    client_host =
        delegate_->inspectable_tab_proxy()->NewClientHost(tab_uid, this);
    DevToolsManager* manager = DevToolsManager::GetInstance();
    if (manager != NULL) {
      manager->RegisterDevToolsClientHostFor(target_host, client_host);
      response->SetInteger(kResultKey, RESULT_OK);
    } else {
      response->SetInteger(kResultKey, RESULT_DEBUGGER_ERROR);
    }
  } else {
    // DevToolsClientHost for this tab is already registered
    response->SetInteger(kResultKey, RESULT_ILLEGAL_TAB_STATE);
  }
}

// Detaches a remote debugger from the target tab specified by |destination|
// by posting the DevToolsAgentMsg_Detach message and sends a response
// to the remote debugger immediately.
void DebuggerRemoteService::DetachFromTab(const std::string& destination,
                                          DictionaryValue* response) {
  int32 tab_uid = -1;
  base::StringToInt(destination, &tab_uid);
  if (tab_uid == -1) {
    // Bad tab_uid received from remote debugger (NaN)
    if (response != NULL) {
      response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    }
    return;
  }
  int result_code;
  DevToolsClientHostImpl* client_host =
      delegate_->inspectable_tab_proxy()->ClientHostForTabId(tab_uid);
  if (client_host != NULL) {
    client_host->Close();
    result_code = RESULT_OK;
  } else {
    // No client host registered for |tab_uid|.
    result_code = RESULT_UNKNOWN_TAB;
  }
  if (response != NULL) {
    response->SetInteger(kResultKey, result_code);
  }
}

// Sends a V8 debugger command to the target tab V8 debugger.
// Does not send back a response (which is received asynchronously
// through IPC) unless an error occurs before the command has actually
// been sent.
bool DebuggerRemoteService::DispatchDebuggerCommand(int tab_uid,
                                                    DictionaryValue* content,
                                                    DictionaryValue* response) {
  if (tab_uid == -1) {
    // Invalid tab_uid from remote debugger (perhaps NaN)
    response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    return true;
  }
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager == NULL) {
    response->SetInteger(kResultKey, RESULT_DEBUGGER_ERROR);
    return true;
  }
  TabContents* tab_contents = ToTabContents(tab_uid);
  if (tab_contents == NULL) {
    // Unknown tab_uid from remote debugger
    response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    return true;
  }
  DevToolsClientHost* client_host =
      manager->GetDevToolsClientHostFor(tab_contents->render_view_host());
  if (client_host == NULL) {
    // tab_uid is not being debugged (Attach has not been invoked)
    response->SetInteger(kResultKey, RESULT_ILLEGAL_TAB_STATE);
    return true;
  }
  std::string v8_command;
  DictionaryValue* v8_command_value;
  content->GetDictionary(kDataKey, &v8_command_value);
  base::JSONWriter::Write(v8_command_value, false, &v8_command);
  manager->ForwardToDevToolsAgent(
      client_host, DevToolsAgentMsg_DebuggerCommand(v8_command));
  // Do not send the response right now, as the JSON will be received from
  // the V8 debugger asynchronously.
  return false;
}

// Sends the immediate "evaluate Javascript" command to the V8 debugger.
// The evaluation result is not sent back to the client as this command
// is in fact needed to invoke processing of queued debugger commands.
bool DebuggerRemoteService::DispatchEvaluateJavascript(
    int tab_uid,
    DictionaryValue* content,
    DictionaryValue* response) {
  if (tab_uid == -1) {
    // Invalid tab_uid from remote debugger (perhaps NaN)
    response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    return true;
  }
  TabContents* tab_contents = ToTabContents(tab_uid);
  if (tab_contents == NULL) {
    // Unknown tab_uid from remote debugger
    response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    return true;
  }
  RenderViewHost* render_view_host = tab_contents->render_view_host();
  if (render_view_host == NULL) {
    // No RenderViewHost
    response->SetInteger(kResultKey, RESULT_UNKNOWN_TAB);
    return true;
  }
  std::string javascript;
  content->GetString(kDataKey, &javascript);
  render_view_host->ExecuteJavascriptInWebFrame(string16(),
                                                UTF8ToUTF16(javascript));
  return false;
}

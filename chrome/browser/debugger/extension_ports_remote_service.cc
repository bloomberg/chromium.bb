// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the ExtensionPortsRemoteService.

// Inspired significantly from debugger_remote_service
// and ../automation/extension_port_container.

#include "chrome/browser/debugger/extension_ports_remote_service.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote_message.h"
#include "chrome/browser/debugger/inspectable_tab_proxy.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

// Protocol is as follows:
//
// From external client:
//   {"command": "connect",
//    "data": {
//      "extensionId": "<extension_id string>",
//      "channelName": "<port name string>",  (optional)
//      "tabId": <numerical tab ID>  (optional)
//    }
//   }
// To connect to a background page or tool strip, the tabId should be omitted.
// Tab IDs can be enumerated with the list_tabs DevToolsService command.
//
// Response:
//   {"command": "connect",
//    "result": 0,  (assuming success)
//    "data": {
//      "portId": <numerical port ID>
//    }
//   }
//
// Posting a message from external client:
// Put the target message port ID in the devtools destination field.
//   {"command": "postMessage",
//    "data": <message body - arbitrary JSON>
//   }
// Response:
//   {"command": "postMessage",
//    "result": 0  (Assuming success)
//   }
// Note this is a confirmation from the devtools protocol layer, not
// a response from the extension.
//
// Message from an extension to the external client:
// The message port ID is in the devtools destination field.
//   {"command": "onMessage",
//    "result": 0,  (Always 0)
//    "data": <message body - arbitrary JSON>
//   }
//
// The "disconnect" command from the external client, and
// "onDisconnect" notification from the ExtensionMessageService, are
// similar: with the message port ID in the destination field, but no
// "data" field in this case.

// Commands:
const char kConnect[] = "connect";
const char kDisconnect[] = "disconnect";
const char kPostMessage[] = "postMessage";
// Events:
const char kOnMessage[] = "onMessage";
const char kOnDisconnect[] = "onDisconnect";

// Constants for the JSON message fields.
// The type is wstring because the constant is used to get a
// DictionaryValue field (which requires a wide string).

// Mandatory.
const char kCommandKey[] = "command";

// Always present in messages sent to the external client.
const char kResultKey[] = "result";

// Field for command-specific parameters. Not strictly necessary, but
// makes it more similar to the remote debugger protocol, which should
// allow easier reuse of client code.
const char kDataKey[] = "data";

// Fields within the "data" dictionary:

// Required for "connect":
const char kExtensionIdKey[] = "extensionId";
// Optional in "connect":
const char kChannelNameKey[] = "channelName";
const char kTabIdKey[] = "tabId";

// Present under "data" in replies to a successful "connect" .
const char kPortIdKey[] = "portId";

}  // namespace

const std::string ExtensionPortsRemoteService::kToolName = "ExtensionPorts";

ExtensionPortsRemoteService::ExtensionPortsRemoteService(
    DevToolsProtocolHandler* delegate)
    : delegate_(delegate), service_(NULL) {
  // We need an ExtensionMessageService instance. It hangs off of
  // |profile|. But we do not have a particular tab or RenderViewHost
  // as context. I'll just use the first active profile not in
  // incognito mode. But this is probably not the right way.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    LOG(WARNING) << "No profile manager for ExtensionPortsRemoteService";
    return;
  }
  for (ProfileManager::ProfileVector::const_iterator it
           = profile_manager->begin();
       it != profile_manager->end();
       ++it) {
    if (!(*it)->IsOffTheRecord()) {
      service_ = (*it)->GetExtensionMessageService();
      break;
    }
  }
  if (!service_)
    LOG(WARNING) << "No usable profile for ExtensionPortsRemoteService";
}

ExtensionPortsRemoteService::~ExtensionPortsRemoteService() {
}

void ExtensionPortsRemoteService::HandleMessage(
    const DevToolsRemoteMessage& message) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  const std::string destinationString = message.destination();
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

  if (!service_) {
    // This happens if we failed to obtain an ExtensionMessageService
    // during initialization.
    NOTREACHED();
    response.SetInteger(kResultKey, RESULT_NO_SERVICE);
    SendResponse(response, message.tool(), message.destination());
    return;
  }

  int destination = -1;
  if (destinationString.size() != 0)
    base::StringToInt(destinationString, &destination);

  if (command == kConnect) {
    if (destination != -1)  // destination should be empty for this command.
      response.SetInteger(kResultKey, RESULT_UNKNOWN_COMMAND);
    else
      ConnectCommand(content, &response);
  } else if (command == kDisconnect) {
    if (destination == -1)  // Destination required for this command.
      response.SetInteger(kResultKey, RESULT_UNKNOWN_COMMAND);
    else
      DisconnectCommand(destination, &response);
  } else if (command == kPostMessage) {
    if (destination == -1)  // Destination required for this command.
      response.SetInteger(kResultKey, RESULT_UNKNOWN_COMMAND);
    else
      PostMessageCommand(destination, content, &response);
  } else {
    // Unknown command
    NOTREACHED();
    response.SetInteger(kResultKey, RESULT_UNKNOWN_COMMAND);
  }
  SendResponse(response, message.tool(), message.destination());
}

void ExtensionPortsRemoteService::OnConnectionLost() {
  VLOG(1) << "OnConnectionLost";
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(service_);
  for (PortIdSet::iterator it = openPortIds_.begin();
      it != openPortIds_.end();
      ++it)
    service_->CloseChannel(*it);
  openPortIds_.clear();
}

void ExtensionPortsRemoteService::SendResponse(
    const Value& response, const std::string& tool,
    const std::string& destination) {
  std::string response_content;
  base::JSONWriter::Write(&response, false, &response_content);
  scoped_ptr<DevToolsRemoteMessage> response_message(
      DevToolsRemoteMessageBuilder::instance().Create(
          tool, destination, response_content));
  delegate_->Send(*response_message.get());
}

bool ExtensionPortsRemoteService::Send(IPC::Message *message) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  IPC_BEGIN_MESSAGE_MAP(ExtensionPortsRemoteService, *message)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionMessageInvoke,
                        OnExtensionMessageInvoke)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  delete message;
  return true;
}

void ExtensionPortsRemoteService::OnExtensionMessageInvoke(
    const std::string& extension_id,
    const std::string& function_name,
    const ListValue& args,
    const GURL& event_url) {
  if (function_name == ExtensionMessageService::kDispatchOnMessage) {
    DCHECK_EQ(args.GetSize(), 2u);
    std::string message;
    int port_id;
    if (args.GetString(0, &message) && args.GetInteger(1, &port_id))
      OnExtensionMessage(message, port_id);
  } else if (function_name == ExtensionMessageService::kDispatchOnDisconnect) {
    DCHECK_EQ(args.GetSize(), 1u);
    int port_id;
    if (args.GetInteger(0, &port_id))
      OnExtensionPortDisconnected(port_id);
  } else if (function_name == ExtensionMessageService::kDispatchOnConnect) {
    // There is no way for this service to be addressed and receive
    // connections.
    NOTREACHED() << function_name << " shouldn't be called.";
  } else {
    NOTREACHED() << function_name << " shouldn't be called.";
  }
}

void ExtensionPortsRemoteService::OnExtensionMessage(
    const std::string& message, int port_id) {
  VLOG(1) << "Message event: from port " << port_id << ", < " << message << ">";
  // Transpose the information into a JSON message for the external client.
  DictionaryValue content;
  content.SetString(kCommandKey, kOnMessage);
  content.SetInteger(kResultKey, RESULT_OK);
  // Turn the stringified message body back into JSON.
  Value* data = base::JSONReader::Read(message, false);
  if (!data) {
    NOTREACHED();
    return;
  }
  content.Set(kDataKey, data);
  SendResponse(content, kToolName, base::IntToString(port_id));
}

void ExtensionPortsRemoteService::OnExtensionPortDisconnected(int port_id) {
  VLOG(1) << "Disconnect event for port " << port_id;
  openPortIds_.erase(port_id);
  DictionaryValue content;
  content.SetString(kCommandKey, kOnDisconnect);
  content.SetInteger(kResultKey, RESULT_OK);
  SendResponse(content, kToolName, base::IntToString(port_id));
}

void ExtensionPortsRemoteService::ConnectCommand(
    DictionaryValue* content, DictionaryValue* response) {
  // Parse out the parameters.
  DictionaryValue* data;
  if (!content->GetDictionary(kDataKey, &data)) {
    response->SetInteger(kResultKey, RESULT_PARAMETER_ERROR);
    return;
  }
  std::string extension_id;
  if (!data->GetString(kExtensionIdKey, &extension_id)) {
    response->SetInteger(kResultKey, RESULT_PARAMETER_ERROR);
    return;
  }
  std::string channel_name = "";
  data->GetString(kChannelNameKey, &channel_name);  // optional.
  int tab_id = -1;
  data->GetInteger(kTabIdKey, &tab_id);  // optional.
  int port_id;
  if (tab_id != -1) {  // Resolve the tab ID.
    const InspectableTabProxy::ControllersMap& navcon_map =
        delegate_->inspectable_tab_proxy()->controllers_map();
    InspectableTabProxy::ControllersMap::const_iterator it =
        navcon_map.find(tab_id);
    TabContents* tab_contents = NULL;
    if (it != navcon_map.end())
      tab_contents = it->second->tab_contents();
    if (!tab_contents) {
      VLOG(1) << "tab not found: " << tab_id;
      response->SetInteger(kResultKey, RESULT_TAB_NOT_FOUND);
      return;
    }
    // Ask the ExtensionMessageService to open the channel.
    VLOG(1) << "Connect: extension_id <" << extension_id
            << ">, channel_name <" << channel_name
            << ">, tab " << tab_id;
    DCHECK(service_);
    port_id = service_->OpenSpecialChannelToTab(
        extension_id, channel_name, tab_contents, this);
  } else {  // no tab: channel to an extension' background page / toolstrip.
    // Ask the ExtensionMessageService to open the channel.
    VLOG(1) << "Connect: extension_id <" << extension_id
            << ">, channel_name <" << channel_name << ">";
    DCHECK(service_);
    port_id = service_->OpenSpecialChannelToExtension(
        extension_id, channel_name, "null", this);
  }
  if (port_id == -1) {
    // Failure: probably the extension ID doesn't exist.
    VLOG(1) << "Connect failed";
    response->SetInteger(kResultKey, RESULT_CONNECT_FAILED);
    return;
  }
  VLOG(1) << "Connected: port " << port_id;
  openPortIds_.insert(port_id);
  // Reply to external client with the port ID assigned to the new channel.
  DictionaryValue* reply_data = new DictionaryValue();
  reply_data->SetInteger(kPortIdKey, port_id);
  response->Set(kDataKey, reply_data);
  response->SetInteger(kResultKey, RESULT_OK);
}

void ExtensionPortsRemoteService::DisconnectCommand(
    int port_id, DictionaryValue* response) {
  VLOG(1) << "Disconnect port " << port_id;
  PortIdSet::iterator portEntry = openPortIds_.find(port_id);
  if (portEntry == openPortIds_.end()) {  // unknown port ID.
    VLOG(1) << "unknown port: " << port_id;
    response->SetInteger(kResultKey, RESULT_UNKNOWN_PORT);
    return;
  }
  DCHECK(service_);
  service_->CloseChannel(port_id);
  openPortIds_.erase(portEntry);
  response->SetInteger(kResultKey, RESULT_OK);
}

void ExtensionPortsRemoteService::PostMessageCommand(
    int port_id, DictionaryValue* content, DictionaryValue* response) {
  Value* data;
  if (!content->Get(kDataKey, &data)) {
    response->SetInteger(kResultKey, RESULT_PARAMETER_ERROR);
    return;
  }
  std::string message;
  // Stringified the JSON message body.
  base::JSONWriter::Write(data, false, &message);
  VLOG(1) << "postMessage: port " << port_id
          << ", message: <" << message << ">";
  PortIdSet::iterator portEntry = openPortIds_.find(port_id);
  if (portEntry == openPortIds_.end()) {  // Unknown port ID.
    VLOG(1) << "unknown port: " << port_id;
    response->SetInteger(kResultKey, RESULT_UNKNOWN_PORT);
    return;
  }
  // Post the message through the ExtensionMessageService.
  DCHECK(service_);
  service_->PostMessageFromRenderer(port_id, message);
  // Confirm to the external client that we sent its message.
  response->SetInteger(kResultKey, RESULT_OK);
}

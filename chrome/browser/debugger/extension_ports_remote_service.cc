// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the ExtensionPortsRemoteService.

// Inspired significantly from debugger_remote_service
// and ../automation/extension_port_container.

#include "chrome/browser/debugger/extension_ports_remote_service.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote_message.h"
#include "chrome/browser/debugger/inspectable_tab_proxy.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

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
static const std::string kConnect = "connect";
static const std::string kDisconnect = "disconnect";
static const std::string kPostMessage = "postMessage";
// Events:
static const std::string kOnMessage = "onMessage";
static const std::string kOnDisconnect = "onDisconnect";

// Constants for the JSON message fields.
// The type is wstring because the constant is used to get a
// DictionaryValue field (which requires a wide string).

// Mandatory.
static const std::wstring kCommandWide = L"command";

// Always present in messages sent to the external client.
static const std::wstring kResultWide = L"result";

// Field for command-specific parameters. Not strictly necessary, but
// makes it more similar to the remote debugger protocol, which should
// allow easier reuse of client code.
static const std::wstring kDataWide = L"data";

// Fields within the "data" dictionary:

// Required for "connect":
static const std::wstring kExtensionIdWide = L"extensionId";
// Optional in "connect":
static const std::wstring kChannelNameWide = L"channelName";
static const std::wstring kTabIdWide = L"tabId";

// Present under "data" in replies to a successful "connect" .
static const std::wstring kPortIdWide = L"portId";

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
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_PORT_DELETED_DEBUG,
      Source<IPC::Message::Sender>(this),
      NotificationService::NoDetails());
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
  if (!content->HasKey(kCommandWide)) {
    NOTREACHED();  // Broken protocol :(
    return;
  }
  std::string command;
  DictionaryValue response;

  content->GetString(kCommandWide, &command);
  response.SetString(kCommandWide, command);

  if (!service_) {
    // This happens if we failed to obtain an ExtensionMessageService
    // during initialization.
    NOTREACHED();
    response.SetInteger(kResultWide, RESULT_NO_SERVICE);
    SendResponse(response, message.tool(), message.destination());
    return;
  }

  int destination = -1;
  if (destinationString.size() != 0)
    StringToInt(destinationString, &destination);

  if (command == kConnect) {
    if (destination != -1)  // destination should be empty for this command.
      response.SetInteger(kResultWide, RESULT_UNKNOWN_COMMAND);
    else
      ConnectCommand(content, &response);
  } else if (command == kDisconnect) {
    if (destination == -1)  // Destination required for this command.
      response.SetInteger(kResultWide, RESULT_UNKNOWN_COMMAND);
    else
      DisconnectCommand(destination, &response);
  } else if (command == kPostMessage) {
    if (destination == -1)  // Destination required for this command.
      response.SetInteger(kResultWide, RESULT_UNKNOWN_COMMAND);
    else
      PostMessageCommand(destination, content, &response);
  } else {
    // Unknown command
    NOTREACHED();
    response.SetInteger(kResultWide, RESULT_UNKNOWN_COMMAND);
  }
  SendResponse(response, message.tool(), message.destination());
}

void ExtensionPortsRemoteService::OnConnectionLost() {
  LOG(INFO) << "OnConnectionLost";
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
    const std::string& function_name, const ListValue& args) {
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
  LOG(INFO) << "Message event: from port " << port_id
            << ", < " << message << ">";
  // Transpose the information into a JSON message for the external client.
  DictionaryValue content;
  content.SetString(kCommandWide, kOnMessage);
  content.SetInteger(kResultWide, RESULT_OK);
  // Turn the stringified message body back into JSON.
  Value* data = base::JSONReader::Read(message, false);
  if (!data) {
    NOTREACHED();
    return;
  }
  content.Set(kDataWide, data);
  SendResponse(content, kToolName, IntToString(port_id));
}

void ExtensionPortsRemoteService::OnExtensionPortDisconnected(int port_id) {
  LOG(INFO) << "Disconnect event for port " << port_id;
  openPortIds_.erase(port_id);
  DictionaryValue content;
  content.SetString(kCommandWide, kOnDisconnect);
  content.SetInteger(kResultWide, RESULT_OK);
  SendResponse(content, kToolName, IntToString(port_id));
}

void ExtensionPortsRemoteService::ConnectCommand(
    DictionaryValue* content, DictionaryValue* response) {
  // Parse out the parameters.
  DictionaryValue* data;
  if (!content->GetDictionary(kDataWide, &data)) {
    response->SetInteger(kResultWide, RESULT_PARAMETER_ERROR);
    return;
  }
  std::string extension_id;
  if (!data->GetString(kExtensionIdWide, &extension_id)) {
    response->SetInteger(kResultWide, RESULT_PARAMETER_ERROR);
    return;
  }
  std::string channel_name = "";
  data->GetString(kChannelNameWide, &channel_name);  // optional.
  int tab_id = -1;
  data->GetInteger(kTabIdWide, &tab_id);  // optional.
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
      LOG(INFO) << "tab not found: " << tab_id;
      response->SetInteger(kResultWide, RESULT_TAB_NOT_FOUND);
      return;
    }
    // Ask the ExtensionMessageService to open the channel.
    LOG(INFO) << "Connect: extension_id <" << extension_id
              << ">, channel_name <" << channel_name << ">"
              << ", tab " << tab_id;
    DCHECK(service_);
    port_id = service_->OpenSpecialChannelToTab(
        extension_id, channel_name, tab_contents, this);
  } else {  // no tab: channel to an extension' background page / toolstrip.
    // Ask the ExtensionMessageService to open the channel.
    LOG(INFO) << "Connect: extension_id <" << extension_id
              << ">, channel_name <" << channel_name << ">";
    DCHECK(service_);
    port_id = service_->OpenSpecialChannelToExtension(
        extension_id, channel_name, this);
  }
  if (port_id == -1) {
    // Failure: probably the extension ID doesn't exist.
    LOG(INFO) << "Connect failed";
    response->SetInteger(kResultWide, RESULT_CONNECT_FAILED);
    return;
  }
  LOG(INFO) << "Connected: port " << port_id;
  openPortIds_.insert(port_id);
  // Reply to external client with the port ID assigned to the new channel.
  DictionaryValue* reply_data = new DictionaryValue();
  reply_data->SetInteger(kPortIdWide, port_id);
  response->Set(kDataWide, reply_data);
  response->SetInteger(kResultWide, RESULT_OK);
}

void ExtensionPortsRemoteService::DisconnectCommand(
    int port_id, DictionaryValue* response) {
  LOG(INFO) << "Disconnect port " << port_id;
  PortIdSet::iterator portEntry = openPortIds_.find(port_id);
  if (portEntry == openPortIds_.end()) {  // unknown port ID.
    LOG(INFO) << "unknown port: " << port_id;
    response->SetInteger(kResultWide, RESULT_UNKNOWN_PORT);
    return;
  }
  DCHECK(service_);
  service_->CloseChannel(port_id);
  openPortIds_.erase(portEntry);
  response->SetInteger(kResultWide, RESULT_OK);
}

void ExtensionPortsRemoteService::PostMessageCommand(
    int port_id, DictionaryValue* content, DictionaryValue* response) {
  Value* data;
  if (!content->Get(kDataWide, &data)) {
    response->SetInteger(kResultWide, RESULT_PARAMETER_ERROR);
    return;
  }
  std::string message;
  // Stringified the JSON message body.
  base::JSONWriter::Write(data, false, &message);
  LOG(INFO) << "postMessage: port " << port_id
            << ", message: <" << message << ">";
  PortIdSet::iterator portEntry = openPortIds_.find(port_id);
  if (portEntry == openPortIds_.end()) {  // Unknown port ID.
    LOG(INFO) << "unknown port: " << port_id;
    response->SetInteger(kResultWide, RESULT_UNKNOWN_PORT);
    return;
  }
  // Post the message through the ExtensionMessageService.
  DCHECK(service_);
  service_->PostMessageFromRenderer(port_id, message);
  // Confirm to the external client that we sent its message.
  response->SetInteger(kResultWide, RESULT_OK);
}

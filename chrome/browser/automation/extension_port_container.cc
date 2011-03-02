// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/extension_port_container.h"

#include "base/logging.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"

// TODO(siggi): Find a more structured way to read and write JSON messages.

namespace ext = extension_automation_constants;

ExtensionPortContainer::ExtensionPortContainer(AutomationProvider* automation,
                                               int tab_handle) :
    automation_(automation), service_(NULL), port_id_(-1),
    tab_handle_(tab_handle) {
  service_ = automation_->profile()->GetExtensionMessageService();
  DCHECK(service_);
}

ExtensionPortContainer::~ExtensionPortContainer() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  if (port_id_ != -1)
    service_->CloseChannel(port_id_);
}

bool ExtensionPortContainer::PostResponseToExternalPort(
    const std::string& message) {
  return automation_->Send(
      new AutomationMsg_ForwardMessageToExternalHost(
          tab_handle_, message, ext::kAutomationOrigin,
          ext::kAutomationPortResponseTarget));
}

bool ExtensionPortContainer::PostMessageToExternalPort(
    const std::string& message) {
  return automation_->Send(
      new AutomationMsg_ForwardMessageToExternalHost(
          tab_handle_, message,
          ext::kAutomationOrigin,
          ext::kAutomationPortRequestTarget));
}

void ExtensionPortContainer::PostMessageFromExternalPort(
    const std::string &message) {
  service_->PostMessageFromRenderer(port_id_, message);
}

bool ExtensionPortContainer::Connect(const std::string &extension_id,
                                     int process_id,
                                     int routing_id,
                                     int connection_id,
                                     const std::string& channel_name,
                                     const std::string& tab_json) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  port_id_ = service_->OpenSpecialChannelToExtension(
      extension_id, channel_name, tab_json, this);
  if (port_id_ == -1) {
    // In this case a disconnect message has been dispatched.
    return false;
  }

  SendConnectionResponse(connection_id, port_id_);
  return true;
}

void ExtensionPortContainer::SendConnectionResponse(int connection_id,
                                                    int port_id) {
  // Compose the reply message.
  scoped_ptr<DictionaryValue> msg_dict(new DictionaryValue());
  msg_dict->SetInteger(ext::kAutomationRequestIdKey, ext::CHANNEL_OPENED);
  msg_dict->SetInteger(ext::kAutomationConnectionIdKey, connection_id);
  msg_dict->SetInteger(ext::kAutomationPortIdKey, port_id);

  std::string msg_json;
  base::JSONWriter::Write(msg_dict.get(), false, &msg_json);

  PostResponseToExternalPort(msg_json);
}

bool ExtensionPortContainer::Send(IPC::Message *message) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  IPC_BEGIN_MESSAGE_MAP(ExtensionPortContainer, *message)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionMessageInvoke,
                        OnExtensionMessageInvoke)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  delete message;
  return true;
}

void ExtensionPortContainer::OnExtensionMessageInvoke(
    const std::string& extension_id,
    const std::string& function_name,
    const ListValue& args,
    const GURL& event_url) {
  if (function_name == ExtensionMessageService::kDispatchOnMessage) {
    DCHECK_EQ(args.GetSize(), 2u);

    std::string message;
    int source_port_id;
    if (args.GetString(0, &message) && args.GetInteger(1, &source_port_id))
      OnExtensionHandleMessage(message, source_port_id);
  } else if (function_name == ExtensionMessageService::kDispatchOnDisconnect) {
    DCHECK_EQ(args.GetSize(), 2u);
    int port_id;
    if (args.GetInteger(0, &port_id))
      OnExtensionPortDisconnected(port_id);
  } else if (function_name == ExtensionMessageService::kDispatchOnConnect) {
    // Do nothing.
    // TODO(siggi): implement
  } else {
    NOTREACHED() << function_name << " shouldn't be called.";
  }
}

void ExtensionPortContainer::OnExtensionHandleMessage(
    const std::string& message, int source_port_id) {
  // Compose the reply message and fire it away.
  DictionaryValue msg_dict;
  msg_dict.SetInteger(ext::kAutomationRequestIdKey, ext::POST_MESSAGE);
  msg_dict.SetInteger(ext::kAutomationPortIdKey, port_id_);
  msg_dict.SetString(ext::kAutomationMessageDataKey, message);

  std::string msg_json;
  base::JSONWriter::Write(&msg_dict, false, &msg_json);

  PostMessageToExternalPort(msg_json);
}

void ExtensionPortContainer::OnExtensionPortDisconnected(int source_port_id) {
  // Compose the disconnect message and fire it away.
  DictionaryValue msg_dict;
  msg_dict.SetInteger(ext::kAutomationRequestIdKey, ext::CHANNEL_CLOSED);
  msg_dict.SetInteger(ext::kAutomationPortIdKey, port_id_);

  std::string msg_json;
  base::JSONWriter::Write(&msg_dict, false, &msg_json);

  PostMessageToExternalPort(msg_json);
}

bool ExtensionPortContainer::InterceptMessageFromExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target, AutomationProvider* automation,
    RenderViewHost *view_host, int tab_handle) {
  if (target != ext::kAutomationPortRequestTarget)
    return false;

  if (origin != ext::kAutomationOrigin) {
    // TODO(siggi): Should we block the message on wrong origin?
    LOG(WARNING) << "Wrong origin on automation port message " << origin;
  }

  scoped_ptr<Value> message_value(base::JSONReader::Read(message, false));
  DCHECK(message_value->IsType(Value::TYPE_DICTIONARY));
  if (!message_value->IsType(Value::TYPE_DICTIONARY))
    return true;

  DictionaryValue* message_dict =
      reinterpret_cast<DictionaryValue*>(message_value.get());

  int command = -1;
  bool got_value = message_dict->GetInteger(ext::kAutomationRequestIdKey,
                                            &command);
  DCHECK(got_value);
  if (!got_value)
    return true;

  if (command == ext::OPEN_CHANNEL) {
    // Extract the "extension_id" and "connection_id" parameters.
    std::string extension_id;
    got_value = message_dict->GetString(ext::kAutomationExtensionIdKey,
                                        &extension_id);
    DCHECK(got_value);
    if (!got_value)
      return true;

    int connection_id;
    got_value = message_dict->GetInteger(ext::kAutomationConnectionIdKey,
                                         &connection_id);
    DCHECK(got_value);
    if (!got_value)
      return true;

    std::string channel_name;
    // Channel name is optional.
    message_dict->GetString(ext::kAutomationChannelNameKey, &channel_name);

    // Tab information is optional, try to retrieve it
    // and re-flatten it to a string.
    std::string tab_json("null");
    DictionaryValue* tab = NULL;
    if (message_dict->GetDictionary(ext::kAutomationTabJsonKey, &tab))
      base::JSONWriter::Write(tab, false, &tab_json);

    int routing_id = view_host->routing_id();
    // Create the extension port and connect it.
    scoped_ptr<ExtensionPortContainer> port(
        new ExtensionPortContainer(automation, tab_handle));

    int process_id = view_host->process()->id();
    if (port->Connect(extension_id, process_id, routing_id, connection_id,
                      channel_name, tab_json)) {
      // We have a successful connection.
      automation->AddPortContainer(port.release());
    }
  } else if (command == ext::POST_MESSAGE) {
    int port_id = -1;
    got_value = message_dict->GetInteger(ext::kAutomationPortIdKey, &port_id);
    DCHECK(got_value);
    if (!got_value)
      return true;

    std::string data;
    got_value = message_dict->GetString(ext::kAutomationMessageDataKey, &data);
    DCHECK(got_value);
    if (!got_value)
      return true;

    ExtensionPortContainer* port = automation->GetPortContainer(port_id);
    DCHECK(port);
    if (port)
      port->PostMessageFromExternalPort(data);
  } else if (command == ext::CHANNEL_CLOSED) {
    int port_id = -1;
    got_value = message_dict->GetInteger(ext::kAutomationPortIdKey, &port_id);
    DCHECK(got_value);
    if (!got_value)
      return true;

    ExtensionPortContainer* port = automation->GetPortContainer(port_id);
    DCHECK(port);
    if (port) {
      // This will delete the port and notify the other end of the disconnect.
      automation->RemovePortContainer(port);
    }
  } else {
    // We don't expect other messages here.
    NOTREACHED();
  }

  return true;
}

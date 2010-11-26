// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Extension port manager takes care of managing the state of
// connecting and connected ports.
#include "ceee/ie/plugin/bho/extension_port_manager.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "ceee/ie/common/chrome_frame_host.h"
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "chrome/browser/automation/extension_automation_constants.h"

namespace ext = extension_automation_constants;

ExtensionPortManager::ExtensionPortManager() {
}

ExtensionPortManager::~ExtensionPortManager() {
}

void ExtensionPortManager::Initialize(IChromeFrameHost* chrome_frame_host) {
  chrome_frame_host_ = chrome_frame_host;
}

void ExtensionPortManager::CloseAll(IContentScriptNativeApi* instance) {
  DCHECK(instance != NULL);

  // TODO(siggi@chromium.org): Deal better with these cases. Connected
  //    ports probably ought to be closed, and connecting ports may
  //    need to hang around until we get the connected message, to be
  //    terminated at that point.
  ConnectedPortMap::iterator it(connected_ports_.begin());
  while (it != connected_ports_.end()) {
    if (it->second.instance = instance) {
      connected_ports_.erase(it++);
    } else {
      ++it;
    }
  }

  ConnectingPortMap::iterator jt(connecting_ports_.begin());
  while (jt != connecting_ports_.end()) {
    if (jt->second.instance = instance) {
      connecting_ports_.erase(jt++);
    } else {
      ++jt;
    }
  }
}

HRESULT ExtensionPortManager::OpenChannelToExtension(
    IContentScriptNativeApi* instance, const std::string& extension,
    const std::string& channel_name, Value* tab, int cookie) {
  int connection_id = next_connection_id_++;

  // Prepare the connection request.
  scoped_ptr<DictionaryValue> dict(new DictionaryValue());
  if (dict.get() == NULL)
    return E_OUTOFMEMORY;

  dict->SetInteger(ext::kAutomationRequestIdKey, ext::OPEN_CHANNEL);
  dict->SetInteger(ext::kAutomationConnectionIdKey, connection_id);
  dict->SetString(ext::kAutomationExtensionIdKey, extension);
  dict->SetString(ext::kAutomationChannelNameKey, channel_name);
  dict->Set(ext::kAutomationTabJsonKey, tab);

  // JSON encode it.
  std::string request_json;
  base::JSONWriter::Write(dict.get(), false, &request_json);
  // And fire it off.
  HRESULT hr = PostMessageToHost(request_json,
                                 ext::kAutomationPortRequestTarget);
  if (FAILED(hr))
    return hr;

  ConnectingPort connecting_port = { instance, cookie };
  connecting_ports_[connection_id] = connecting_port;

  return S_OK;
}

HRESULT ExtensionPortManager::PostMessage(int port_id,
                                          const std::string& message) {
  // Wrap the message for sending as a port request.
  scoped_ptr<DictionaryValue> dict(new DictionaryValue());
  if (dict.get() == NULL)
    return E_OUTOFMEMORY;

  dict->SetInteger(ext::kAutomationRequestIdKey, ext::POST_MESSAGE);
  dict->SetInteger(ext::kAutomationPortIdKey, port_id);
  dict->SetString(ext::kAutomationMessageDataKey, message);

  // JSON encode it.
  std::string message_json;
  base::JSONWriter::Write(dict.get(), false, &message_json);

  // And fire it off.
  return PostMessageToHost(message_json,
                           std::string(ext::kAutomationPortRequestTarget));
}

void ExtensionPortManager::OnPortMessage(BSTR message) {
  std::string message_json = CW2A(message);
  scoped_ptr<Value> value(base::JSONReader::Read(message_json, true));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED();
    LOG(ERROR) << "Invalid message";
    return;
  }

  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  int request = -1;
  if (!dict->GetInteger(ext::kAutomationRequestIdKey, &request)) {
    NOTREACHED();
    LOG(ERROR) << "Request ID missing";
    return;
  }

  if (request == ext::CHANNEL_OPENED) {
    int connection_id = -1;
    if (!dict->GetInteger(ext::kAutomationConnectionIdKey, &connection_id)) {
      NOTREACHED();
      LOG(ERROR) << "Connection ID missing";
      return;
    }

    int port_id = -1;
    if (!dict->GetInteger(ext::kAutomationPortIdKey, &port_id)) {
      NOTREACHED();
      LOG(ERROR) << "Port ID missing";
      return;
    }

    ConnectingPortMap::iterator it(connecting_ports_.find(connection_id));
    if (it == connecting_ports_.end()) {
      // TODO(siggi@chromium.org): This can happen legitimately on a
      //    race between connect and document unload. We should
      //    probably respond with a close port message here.
      LOG(ERROR) << "No such connection id " << connection_id;
      return;
    }
    ConnectingPort port = it->second;
    connecting_ports_.erase(it);
    // Did it connect successfully?
    if (port_id != -1)
      connected_ports_[port_id].instance = port.instance;

    port.instance->OnChannelOpened(port.cookie, port_id);
    return;
  } else if (request == ext::POST_MESSAGE) {
    int port_id = -1;
    if (!dict->GetInteger(ext::kAutomationPortIdKey, &port_id)) {
      NOTREACHED();
      LOG(ERROR) << "No port id";
      return;
    }

    std::string data;
    if (!dict->GetString(ext::kAutomationMessageDataKey, &data)) {
      NOTREACHED();
      LOG(ERROR) << "No message data";
      return;
    }

    ConnectedPortMap::iterator it(connected_ports_.find(port_id));
    if (it == connected_ports_.end()) {
      NOTREACHED();
      LOG(ERROR) << "No such port " << port_id;
      return;
    }

    it->second.instance->OnPostMessage(port_id, data);
    return;
  } else if (request == ext::CHANNEL_CLOSED) {
    // TODO(siggi@chromium.org): handle correctly.
    return;
  }

  NOTREACHED();
}

HRESULT ExtensionPortManager::PostMessageToHost(const std::string& message,
                                                const std::string& target) {
  // Post our message through the ChromeFrameHost. We allow queueing,
  // because we don't synchronize to the destination extension loading.
  return chrome_frame_host_->PostMessage(CComBSTR(message.c_str()),
                                         CComBSTR(target.c_str()));
}

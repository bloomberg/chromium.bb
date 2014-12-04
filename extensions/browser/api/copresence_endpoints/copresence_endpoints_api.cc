// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/copresence_endpoints/copresence_endpoints_api.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/copresence_endpoints/public/copresence_endpoint.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/copresence_endpoints/copresence_endpoint_resource.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/copresence_endpoints.h"
#include "net/base/io_buffer.h"

using copresence_endpoints::CopresenceEndpoint;

namespace extensions {

namespace {

const size_t kSizeBytes = 2;
const char kToField[] = "to";
const char kDataField[] = "data";
const char kReplyToField[] = "replyTo";

bool Base64DecodeWithoutPadding(const std::string& data, std::string* out) {
  std::string ret = data;
  while (ret.size() % 4)
    ret.push_back('=');

  if (!base::Base64Decode(ret, &ret))
    return false;

  out->swap(ret);
  return true;
}

std::string Base64EncodeWithoutPadding(const std::string& data) {
  std::string ret = data;
  base::Base64Encode(ret, &ret);
  while (*(ret.end() - 1) == '=')
    ret.erase(ret.end() - 1);
  return ret;
}

// Create a message to send to another endpoint.
std::string CreateMessage(const std::string& data,
                          const std::string& local_endpoint_locator,
                          int remote_endpoint_id) {
  base::DictionaryValue dict;
  dict.SetString(kToField, base::IntToString(remote_endpoint_id));
  dict.SetString(kDataField, Base64EncodeWithoutPadding(data));
  dict.SetString(kReplyToField,
                 Base64EncodeWithoutPadding(local_endpoint_locator));

  std::string json;
  base::JSONWriter::Write(&dict, &json);

  std::string message;
  message.push_back(static_cast<unsigned char>(json.size() & 0xff));
  message.push_back(static_cast<unsigned char>(json.size() >> 8));

  message.append(json);
  return message;
}

bool IsMessageComplete(const std::string& message, size_t* length) {
  if (message.size() < kSizeBytes)
    return false;

  // message[1] = upper order 8 bits.
  // message[0] = lower order 8 bits.
  size_t message_length = (static_cast<size_t>(message[1]) << 8) |
                          (static_cast<size_t>(message[0]) & 0xff);

  if (message.size() >= kSizeBytes + message_length) {
    *length = message_length;
    return true;
  }

  return false;
}

bool ExtractEndpointId(const std::string& endpoint_locator, int* id) {
  std::vector<std::string> tokens;
  if (Tokenize(endpoint_locator, ".", &tokens) <= 1)
    return false;

  if (!base::StringToInt(tokens[0], id))
    return false;

  return true;
}

// Parse a message received from another endpoint.
bool ParseReceivedMessage(const std::string& message,
                          std::string* data,
                          int* remote_endpoint_id) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(message));

  // Check to see that we have a valid dictionary.
  base::DictionaryValue* dict = nullptr;
  if (!value || !value->GetAsDictionary(&dict) || !dict->HasKey(kDataField)) {
    LOG(WARNING) << "Invalid message: " << message;
    return false;
  }

  // The fields in the json string are,
  // to: Endpoint Id this message is meant for (unused atm, we only support one
  //     local endpoint). TODO(rkc): Fix this to support multiple endpoints.
  // data: Data content of the message.
  // replyTo: Sender of this message (in the locator data format). We only need
  //          the endpoint id that we have to reply to, but currently we get
  //          the full locator. TODO(rkc): Fix this once other platforms change
  //          over to only sending us the ID.
  if (!dict->GetStringASCII(kDataField, data))
    return false;
  if (!Base64DecodeWithoutPadding(*data, data))
    return false;

  std::string endpoint_locator;
  if (!dict->GetStringASCII(kReplyToField, &endpoint_locator))
    return false;
  if (!Base64DecodeWithoutPadding(endpoint_locator, &endpoint_locator))
    return false;

  if (!ExtractEndpointId(endpoint_locator, remote_endpoint_id))
    return false;

  VLOG(3) << "Valid message parsed.";
  return true;
}

}  // namespace

// CopresenceEndpointFunction public methods:

CopresenceEndpointFunction::CopresenceEndpointFunction()
    : endpoints_manager_(nullptr) {
}

void CopresenceEndpointFunction::DispatchOnConnectedEvent(int endpoint_id) {
  // Send the messages to the client app.
  scoped_ptr<Event> event(new Event(
      core_api::copresence_endpoints::OnConnected::kEventName,
      core_api::copresence_endpoints::OnConnected::Create(endpoint_id),
      browser_context()));
  EventRouter::Get(browser_context())
      ->DispatchEventToExtension(extension_id(), event.Pass());
  VLOG(2) << "Dispatched OnConnected event: endpointId = " << endpoint_id;
}

// CopresenceEndpointFunction protected methods:

int CopresenceEndpointFunction::AddEndpoint(
    CopresenceEndpointResource* endpoint) {
  return endpoints_manager_->Add(endpoint);
}

void CopresenceEndpointFunction::ReplaceEndpoint(
    const std::string& extension_id,
    int endpoint_id,
    CopresenceEndpointResource* endpoint) {
  endpoints_manager_->Replace(extension_id, endpoint_id, endpoint);
}

CopresenceEndpointResource* CopresenceEndpointFunction::GetEndpoint(
    int endpoint_id) {
  return endpoints_manager_->Get(extension_id(), endpoint_id);
}

void CopresenceEndpointFunction::RemoveEndpoint(int endpoint_id) {
  endpoints_manager_->Remove(extension_id(), endpoint_id);
}

ExtensionFunction::ResponseAction CopresenceEndpointFunction::Run() {
  Initialize();
  return Execute();
}

// CopresenceEndpointFunction private methods:

CopresenceEndpointFunction::~CopresenceEndpointFunction() {
}

void CopresenceEndpointFunction::Initialize() {
  endpoints_manager_ =
      ApiResourceManager<CopresenceEndpointResource>::Get(browser_context());
}

void CopresenceEndpointFunction::OnDataReceived(
    int local_endpoint_id,
    const scoped_refptr<net::IOBuffer>& buffer,
    int size) {
  CopresenceEndpointResource* local_endpoint = GetEndpoint(local_endpoint_id);
  if (!local_endpoint) {
    VLOG(2) << "Receiving endpoint not found. ID = " << local_endpoint_id;
    return;
  }

  std::string& packet = local_endpoint->packet();
  packet.append(std::string(buffer->data(), size));
  size_t message_length;
  if (IsMessageComplete(packet, &message_length)) {
    std::string message_data;
    int remote_endpoint_id;
    if (ParseReceivedMessage(packet.substr(kSizeBytes, message_length),
                             &message_data, &remote_endpoint_id)) {
      DispatchOnReceiveEvent(local_endpoint_id, remote_endpoint_id,
                             message_data);
    } else {
      LOG(WARNING) << "Invalid message received: "
                   << packet.substr(kSizeBytes, message_length)
                   << " of length: " << message_length;
    }

    if (packet.size() > message_length + kSizeBytes) {
      packet = packet.substr(message_length + kSizeBytes);
    } else {
      packet.clear();
    }
  }
}

void CopresenceEndpointFunction::DispatchOnReceiveEvent(
    int local_endpoint_id,
    int remote_endpoint_id,
    const std::string& data) {
  core_api::copresence_endpoints::ReceiveInfo info;
  info.local_endpoint_id = local_endpoint_id;
  info.remote_endpoint_id = remote_endpoint_id;
  info.data = data;
  // Send the data to the client app.
  scoped_ptr<Event> event(
      new Event(core_api::copresence_endpoints::OnReceive::kEventName,
                core_api::copresence_endpoints::OnReceive::Create(info),
                browser_context()));
  EventRouter::Get(browser_context())
      ->DispatchEventToExtension(extension_id(), event.Pass());
  VLOG(2) << "Dispatched OnReceive event: localEndpointId = "
          << local_endpoint_id << ", remoteEndpointId = " << remote_endpoint_id
          << " and data = " << data;
}

// CopresenceEndpointsCreateLocalEndpointFunction implementation:
ExtensionFunction::ResponseAction
CopresenceEndpointsCreateLocalEndpointFunction::Execute() {
  // Add an empty endpoint to create a placeholder endpoint_id. We will need to
  // bind
  // this id to the OnConnected event dispatcher, so we need it before we
  // create the actual endpoint. Once we have the endpoint created, we'll
  // replace the
  // placeholder with the actual endpoint object.
  int endpoint_id =
      AddEndpoint(new CopresenceEndpointResource(extension_id(), nullptr));

  scoped_ptr<CopresenceEndpoint> endpoint =
      make_scoped_ptr(new CopresenceEndpoint(
          endpoint_id,
          base::Bind(&CopresenceEndpointsCreateLocalEndpointFunction::OnCreated,
                     this, endpoint_id),
          base::Bind(&CopresenceEndpointFunction::DispatchOnConnectedEvent,
                     this, endpoint_id),
          base::Bind(&CopresenceEndpointFunction::OnDataReceived, this,
                     endpoint_id)));

  ReplaceEndpoint(
      extension_id(), endpoint_id,
      new CopresenceEndpointResource(extension_id(), endpoint.Pass()));

  return RespondLater();
}

void CopresenceEndpointsCreateLocalEndpointFunction::OnCreated(
    int endpoint_id,
    const std::string& locator) {
  core_api::copresence_endpoints::EndpointInfo endpoint_info;
  endpoint_info.endpoint_id = endpoint_id;
  endpoint_info.locator = locator;
  Respond(ArgumentList(
      core_api::copresence_endpoints::CreateLocalEndpoint::Results::Create(
          endpoint_info)));
}

// CopresenceEndpointsDestroyEndpointFunction implementation:
ExtensionFunction::ResponseAction
CopresenceEndpointsDestroyEndpointFunction::Execute() {
  scoped_ptr<core_api::copresence_endpoints::DestroyEndpoint::Params> params(
      core_api::copresence_endpoints::DestroyEndpoint::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  RemoveEndpoint(params->endpoint_id);
  return RespondNow(NoArguments());
}

// CopresenceEndpointsSendFunction implementation:
ExtensionFunction::ResponseAction CopresenceEndpointsSendFunction::Execute() {
  scoped_ptr<core_api::copresence_endpoints::Send::Params> params(
      core_api::copresence_endpoints::Send::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopresenceEndpointResource* endpoint = GetEndpoint(params->local_endpoint_id);
  if (!endpoint) {
    VLOG(1) << "Endpoint not found. ID = " << params->local_endpoint_id;
    return RespondNow(
        ArgumentList(core_api::copresence_endpoints::Send::Results::Create(
            core_api::copresence_endpoints::
                ENDPOINT_STATUS_INVALID_LOCAL_ENDPOINT)));
  }
  DCHECK(endpoint->endpoint());

  const std::string& message =
      CreateMessage(params->data, endpoint->endpoint()->GetLocator(),
                    params->remote_endpoint_id);
  VLOG(3) << "Sending message to remote_endpoint_id = "
          << params->remote_endpoint_id
          << " from local_endpoint_id = " << params->local_endpoint_id
          << " with data[0] = " << static_cast<int>(message[0])
          << ", data[1] = " << static_cast<int>(message[1]) << ", data[2:] = "
          << std::string((message.c_str() + 2), message.size() - 2);

  endpoint->endpoint()->Send(new net::StringIOBuffer(message), message.size());

  return RespondNow(
      ArgumentList(core_api::copresence_endpoints::Send::Results::Create(
          core_api::copresence_endpoints::ENDPOINT_STATUS_NO_ERROR)));
}

}  // namespace extensions

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_browser_target.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/values.h"
#include "net/server/http_server.h"

namespace {

base::Value* CreateErrorObject(int error_code, const std::string& message) {
  base::DictionaryValue* error_object = new base::DictionaryValue();
  error_object->SetInteger("code", error_code);
  error_object->SetString("message", message);
  return error_object;
}

}  // namespace

namespace content {

DevToolsBrowserTarget::DomainHandler::~DomainHandler() {
}

void DevToolsBrowserTarget::DomainHandler::RegisterCommandHandler(
    const std::string& command,
    CommandHandler handler) {
  command_handlers_[command] = handler;
}

DevToolsBrowserTarget::DomainHandler::DomainHandler(const std::string& domain)
    : domain_(domain) {
}

base::DictionaryValue* DevToolsBrowserTarget::DomainHandler::HandleCommand(
    const std::string& command,
    const base::DictionaryValue* params,
    base::Value** error_out) {
  CommandHandlers::iterator it = command_handlers_.find(command);
  if (it == command_handlers_.end()) {
    *error_out = CreateErrorObject(-1, "Invalid method");
    return NULL;
  }
  return (it->second).Run(params, error_out);
}

void DevToolsBrowserTarget::DomainHandler::SendNotification(
    const std::string& method,
    base::DictionaryValue* params,
    base::Value* error) {
  notifier_.Run(method, params, error);
}

DevToolsBrowserTarget::DevToolsBrowserTarget(
    base::MessageLoopProxy* message_loop_proxy,
    net::HttpServer* http_server,
    int connection_id)
    : message_loop_proxy_(message_loop_proxy),
      http_server_(http_server),
      connection_id_(connection_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

DevToolsBrowserTarget::~DevToolsBrowserTarget() {
  for (DomainHandlerMap::iterator i = handlers_.begin(); i != handlers_.end();
       ++i)
    delete i->second;
}

void DevToolsBrowserTarget::RegisterDomainHandler(DomainHandler* handler) {
  std::string domain = handler->domain();
  DCHECK(handlers_.find(domain) == handlers_.end());
  handlers_[domain] = handler;
  handler->set_notifier(Bind(&DevToolsBrowserTarget::SendNotification,
                             weak_factory_.GetWeakPtr()));
}

std::string DevToolsBrowserTarget::HandleMessage(const std::string& data) {
  int error_code;
  std::string error_message;
  scoped_ptr<base::Value> command(
      base::JSONReader::ReadAndReturnError(
          data, 0, &error_code, &error_message));

  if (!command || !command->IsType(base::Value::TYPE_DICTIONARY))
    return SerializeErrorResponse(
        -1, CreateErrorObject(error_code, error_message));

  int request_id;
  std::string domain;
  std::string method;
  base::DictionaryValue* command_dict = NULL;
  bool ok = true;
  ok &= command->GetAsDictionary(&command_dict);
  ok &= command_dict->GetInteger("id", &request_id);
  ok &= command_dict->GetString("method", &method);
  if (!ok)
    return SerializeErrorResponse(
        request_id, CreateErrorObject(-1, "Malformed request"));

  base::DictionaryValue* params = NULL;
  command_dict->GetDictionary("params", &params);

  size_t pos = method.find(".");
  if (pos == std::string::npos)
    return SerializeErrorResponse(
        request_id, CreateErrorObject(-1, "Method unsupported"));

  domain = method.substr(0, pos);
  if (domain.empty() || handlers_.find(domain) == handlers_.end())
    return SerializeErrorResponse(
        request_id, CreateErrorObject(-1, "Domain unsupported"));

  base::Value* error_object = NULL;
  base::DictionaryValue* domain_result = handlers_[domain]->HandleCommand(
      method, params, &error_object);

  if (error_object)
    return SerializeErrorResponse(request_id, error_object);

  return SerializeResponse(request_id, domain_result);
}

void DevToolsBrowserTarget::SendNotification(const std::string& method,
                                             DictionaryValue* params,
                                             Value* error) {
  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  response->SetString("method", method);
  if (error)
    response->Set("error", error);
  else if (params)
    response->Set("params", params);

  // Serialize response.
  std::string json_response;
  base::JSONWriter::Write(response.get(), &json_response);

  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::SendOverWebSocket,
                 http_server_,
                 connection_id_,
                 json_response));
}

std::string DevToolsBrowserTarget::SerializeErrorResponse(
    int request_id, base::Value* error_object) {
  scoped_ptr<base::DictionaryValue> error_response(new base::DictionaryValue());
  error_response->SetInteger("id", request_id);
  error_response->Set("error", error_object);
  // Serialize response.
  std::string json_response;
  base::JSONWriter::Write(error_response.get(), &json_response);
  return json_response;
}

std::string DevToolsBrowserTarget::SerializeResponse(
    int request_id, base::DictionaryValue* result) {
  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  response->SetInteger("id", request_id);
  if (result)
    response->Set("result", result);

  // Serialize response.
  std::string json_response;
  base::JSONWriter::Write(response.get(), &json_response);
  return json_response;
}

}  // namespace content

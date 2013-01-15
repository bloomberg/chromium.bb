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

namespace content {

namespace {

class NotifierImpl : public DevToolsBrowserTarget::Notifier {
 public:
  NotifierImpl(
      base::MessageLoopProxy* message_loop_proxy,
      net::HttpServer* http_server,
      int connection_id)
     : message_loop_proxy_(message_loop_proxy),
       server_(http_server),
       connection_id_(connection_id) {
  }

  virtual ~NotifierImpl() {}

  // DevToolsBrowserTarget::Notifier
  virtual void Notify(const std::string& data) OVERRIDE {
    message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_,
                   connection_id_,
                   data));
  }

 private:
  base::MessageLoopProxy* const message_loop_proxy_;
  net::HttpServer* const server_;
  const int connection_id_;

  DISALLOW_COPY_AND_ASSIGN(NotifierImpl);
};

}  // namespace

DevToolsBrowserTarget::Handler::Handler()
    : notifier_(NULL) {
}

DevToolsBrowserTarget::Handler::~Handler() {
}

void DevToolsBrowserTarget::Handler::set_notifier(
    DevToolsBrowserTarget::Notifier* notifier) {
  notifier_ = notifier;
}

DevToolsBrowserTarget::Notifier* DevToolsBrowserTarget::Handler::notifier()
    const {
  return notifier_;
}

DevToolsBrowserTarget::DevToolsBrowserTarget(
    base::MessageLoopProxy* message_loop_proxy,
    net::HttpServer* http_server,
    int connection_id)
    : notifier_(
          new NotifierImpl(message_loop_proxy, http_server, connection_id)),
      connection_id_(connection_id) {
}

DevToolsBrowserTarget::~DevToolsBrowserTarget() {
  for (HandlersMap::iterator i = handlers_.begin(); i != handlers_.end(); ++i)
    delete i->second;
}

void DevToolsBrowserTarget::RegisterHandler(Handler* handler) {
  std::string domain = handler->Domain();
  DCHECK(handlers_.find(domain) == handlers_.end());
  handlers_[domain] = handler;
  handler->set_notifier(notifier_.get());
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
  base::Value* domain_result = handlers_[domain]->OnProtocolCommand(
      method, params, &error_object);

  if (error_object)
    return SerializeErrorResponse(request_id, error_object);

  if (!domain_result)
    return SerializeErrorResponse(
        request_id, CreateErrorObject(-1, "Invalid call"));

  DictionaryValue* response = new DictionaryValue();
  response->Set("result", domain_result);
  return SerializeResponse(request_id, response);
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

base::Value* DevToolsBrowserTarget::CreateErrorObject(
    int error_code, const std::string& message) {
  base::DictionaryValue* error_object = new base::DictionaryValue();
  error_object->SetInteger("code", error_code);
  error_object->SetString("message", message);
  return error_object;
}

std::string DevToolsBrowserTarget::SerializeResponse(
    int request_id, base::Value* response) {
  scoped_ptr<base::DictionaryValue> ret(new base::DictionaryValue());
  ret->SetInteger("id", request_id);
  ret->Set("response", response);

  // Serialize response.
  std::string json_response;
  base::JSONWriter::Write(ret.get(), &json_response);
  return json_response;
}

}  // namespace content

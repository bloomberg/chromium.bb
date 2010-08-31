// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/remoting_setup_message_handler.h"

#include "base/scoped_ptr.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "chrome/browser/remoting/remoting_setup_flow.h"

DOMMessageHandler* RemotingSetupMessageHandler::Attach(DOMUI* dom_ui) {
  // Pass the DOMUI object to the setup flow.
  flow_->Attach(dom_ui);
  return DOMMessageHandler::Attach(dom_ui);
}

void RemotingSetupMessageHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("SubmitAuth",
      NewCallback(this, &RemotingSetupMessageHandler::HandleSubmitAuth));
}

void RemotingSetupMessageHandler::HandleSubmitAuth(const ListValue* args) {
  std::string json(dom_ui_util::GetJsonResponseFromFirstArgumentInList(args));
  std::string username, password, captcha;
  if (json.empty())
    return;

  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED() << "Unable to parse auth data";
    return;
  }

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  if (!result->GetString("user", &username) ||
      !result->GetString("pass", &password) ||
      !result->GetString("captcha", &captcha)) {
    NOTREACHED() << "Unable to parse auth data";
    return;
  }

  // Pass the information to the flow.
  if (flow_)
    flow_->OnUserSubmittedAuth(username, password, captcha);
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_setup_message_handler.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"

WebUIMessageHandler* CloudPrintSetupMessageHandler::Attach(WebUI* web_ui) {
  // Pass the WebUI object to the setup flow.
  flow_->Attach(web_ui);
  return WebUIMessageHandler::Attach(web_ui);
}

void CloudPrintSetupMessageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("SubmitAuth",
      NewCallback(this, &CloudPrintSetupMessageHandler::HandleSubmitAuth));
  web_ui_->RegisterMessageCallback("PrintTestPage",
      NewCallback(this, &CloudPrintSetupMessageHandler::HandlePrintTestPage));
  web_ui_->RegisterMessageCallback("LearnMore",
      NewCallback(this, &CloudPrintSetupMessageHandler::HandleLearnMore));
}

void CloudPrintSetupMessageHandler::HandleSubmitAuth(const ListValue* args) {
  std::string json;
  args->GetString(0, &json);
  std::string username, password, captcha, access_code;
  if (json.empty()) {
    NOTREACHED() << "Empty json string";
    return;
  }

  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED() << "Unable to parse auth data";
    return;
  }

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  if (!result->GetString("user", &username) ||
      !result->GetString("pass", &password) ||
      !result->GetString("captcha", &captcha) ||
      !result->GetString("access_code", &access_code)) {
    NOTREACHED() << "Unable to parse auth data";
    return;
  }

  // Pass the information to the flow.
  if (flow_)
    flow_->OnUserSubmittedAuth(username, password, captcha, access_code);
}

void CloudPrintSetupMessageHandler::HandlePrintTestPage(const ListValue* args) {
  if (flow_)
    flow_->OnUserClickedPrintTestPage();
}

void CloudPrintSetupMessageHandler::HandleLearnMore(const ListValue* args) {
  if (flow_)
    flow_->OnUserClickedLearnMore();
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/automation_json_requests.h"

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

namespace {

bool SendAutomationJSONRequest(AutomationMessageSender* sender,
                               const DictionaryValue& request_dict,
                               DictionaryValue* reply_dict) {
  std::string request, reply;
  base::JSONWriter::Write(&request_dict, false, &request);
  bool success = false;
  if (!SendAutomationJSONRequest(sender, request, &reply, &success))
    return false;
  if (!success) {
    std::string command;
    request_dict.GetString("command", &command);
    LOG(ERROR) << "JSON request failed: " << command;
    return false;
  }
  scoped_ptr<Value> value(base::JSONReader::Read(reply, true));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY))
    return false;
  reply_dict->MergeDictionary(static_cast<DictionaryValue*>(value.get()));
  return true;
}

}  // namespace

bool SendAutomationJSONRequest(AutomationMessageSender* sender,
                               const std::string& request,
                               std::string* reply,
                               bool* success) {
  return sender->Send(new AutomationMsg_SendJSONRequest(
      -1, request, reply, success));
}

bool SendGetIndicesFromTabJSONRequest(
    AutomationMessageSender* sender,
    int handle,
    int* browser_index,
    int* tab_index) {
  DictionaryValue request_dict;
  request_dict.SetString("command", "GetIndicesFromTab");
  request_dict.SetInteger("tab_handle", handle);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, request_dict, &reply_dict))
    return false;
  if (!reply_dict.GetInteger("windex", browser_index))
    return false;
  if (!reply_dict.GetInteger("tab_index", tab_index))
    return false;
  return true;
}

bool SendNavigateToURLJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const GURL& url,
    int navigation_count,
    AutomationMsg_NavigationResponseValues* nav_response) {
  DictionaryValue dict;
  dict.SetString("command", "NavigateToURL");
  dict.SetInteger("windex", browser_index);
  dict.SetInteger("tab_index", tab_index);
  dict.SetString("url", url.possibly_invalid_spec());
  dict.SetInteger("navigation_count", navigation_count);
  DictionaryValue reply_dict;
  if (!SendAutomationJSONRequest(sender, dict, &reply_dict))
    return false;
  int response = 0;
  if (!reply_dict.GetInteger("result", &response))
    return false;
  *nav_response = static_cast<AutomationMsg_NavigationResponseValues>(response);
  return true;
}

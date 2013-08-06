// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_response_parser.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"

namespace cloud_print_response_parser {

Job::Job() {
}

Job::~Job() {
}

// Parses json base::Value to base::DictionaryValue.
// If |success| value in JSON dictionary is |false| then |message| will
// contain |message| from the JSON dictionary.
// Returns |true| if no parsing error occurred.
bool GetJsonDictinaryAndCheckSuccess(base::Value* json,
                                     std::string* error_description,
                                     bool* json_success,
                                     std::string* message,
                                     base::DictionaryValue** dictionary) {
  base::DictionaryValue* response_dictionary = NULL;
  if (!json || !json->GetAsDictionary(&response_dictionary)) {
    *error_description = "No JSON dictionary response received.";
    return false;
  }

  bool response_json_success = false;
  if (!response_dictionary->GetBoolean("success", &response_json_success)) {
    *error_description = "Cannot parse success state.";
    return false;
  }

  if (!response_json_success) {
    std::string response_message;
    if (!response_dictionary->GetString("message", &response_message)) {
      *error_description = "Cannot parse message from response.";
      return false;
    }
    *message = response_message;
  }

  *json_success = response_json_success;
  *dictionary = response_dictionary;
  return true;
}

bool ParseRegisterStartResponse(const std::string& response,
                                std::string* error_description,
                                std::string* polling_url,
                                std::string* registration_token,
                                std::string* complete_invite_url,
                                std::string* device_id) {
  scoped_ptr<base::Value> json(base::JSONReader::Read(response));
  base::DictionaryValue* response_dictionary = NULL;
  bool json_success;
  std::string message;
  if (!GetJsonDictinaryAndCheckSuccess(json.get(), error_description,
                                       &json_success, &message,
                                       &response_dictionary)) {
    return false;
  }
  if (!json_success) {
    *error_description = message;
    return false;
  }

  std::string response_registration_token;
  if (!response_dictionary->GetString("registration_token",
                                      &response_registration_token)) {
    *error_description = "No registration_token specified.";
    return false;
  }

  std::string response_complete_invite_url;
  if (!response_dictionary->GetString("complete_invite_url",
                                      &response_complete_invite_url)) {
    *error_description = "No complete_invite_url specified.";
    return false;
  }

  std::string response_polling_url;
  if (!response_dictionary->GetString("polling_url", &response_polling_url)) {
    *error_description = "No polling_url specified.";
    return false;
  }

  base::ListValue* list = NULL;
  if (!response_dictionary->GetList("printers", &list)) {
    *error_description = "No printers list specified.";
    return false;
  }

  base::DictionaryValue* printer = NULL;
  if (!list->GetDictionary(0, &printer)) {
    *error_description = "Printers list is empty or printer is not dictionary.";
    return false;
  }

  std::string id;
  if (!printer->GetString("id", &id)) {
    *error_description = "No id specified.";
    return false;
  }

  if (id.empty()) {
    *error_description = "id is empty.";
    return false;
  }

  *polling_url = response_polling_url;
  *registration_token = response_registration_token;
  *complete_invite_url = response_complete_invite_url;
  *device_id = id;
  return true;
}

bool ParseRegisterCompleteResponse(const std::string& response,
                                   std::string* error_description,
                                   std::string* authorization_code,
                                   std::string* xmpp_jid) {
  scoped_ptr<base::Value> json(base::JSONReader::Read(response));
  base::DictionaryValue* response_dictionary = NULL;
  bool json_success;
  std::string message;
  if (!GetJsonDictinaryAndCheckSuccess(json.get(), error_description,
                                       &json_success, &message,
                                       &response_dictionary)) {
    return false;
  }
  if (!json_success) {
    *error_description = message;
    return false;
  }

  std::string response_authorization_code;
  if (!response_dictionary->GetString("authorization_code",
                                      &response_authorization_code)) {
    *error_description = "Cannot parse authorization_code.";
    return false;
  }

  std::string response_xmpp_jid;
  if (!response_dictionary->GetString("xmpp_jid", &response_xmpp_jid)) {
    *error_description = "Cannot parse xmpp jid.";
    return false;
  }

  *authorization_code = response_authorization_code;
  *xmpp_jid = response_xmpp_jid;
  return true;
}

bool ParseFetchResponse(const std::string& response,
                        std::string* error_description,
                        std::vector<Job>* list) {
  scoped_ptr<base::Value> json(base::JSONReader::Read(response));
  base::DictionaryValue* response_dictionary = NULL;
  bool json_success;
  std::string message;
  if (!GetJsonDictinaryAndCheckSuccess(json.get(), error_description,
                                       &json_success, &message,
                                       &response_dictionary)) {
    return false;
  }

  if (!json_success) {  // Let's suppose we have no jobs to proceed.
    list->clear();
    return true;
  }

  base::ListValue* jobs = NULL;
  if (!response_dictionary->GetList("jobs", &jobs)) {
    *error_description = "Cannot parse jobs list.";
    return false;
  }

  std::vector<Job> job_list(jobs->GetSize());
  for (size_t idx = 0; idx < job_list.size(); ++idx) {
    base::DictionaryValue* job = NULL;
    jobs->GetDictionary(idx, &job);
    if (!job->GetString("id", &job_list[idx].job_id) ||
        !job->GetString("createTime", &job_list[idx].create_time) ||
        !job->GetString("fileUrl", &job_list[idx].file_url) ||
        !job->GetString("ticketUrl", &job_list[idx].ticket_url) ||
        !job->GetString("title", &job_list[idx].title)) {
      *error_description = "Cannot parse job info.";
      return false;
    }
  }

  *list = job_list;

  return true;
}

}  // namespace cloud_print_response_parser


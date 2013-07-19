// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_response_parser.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace cloud_print_response_parser {

// Parses json base::Value to base::DictionaryValue and checks |success| status.
// Returns |true| on success.
bool GetJsonDictinaryAndCheckSuccess(base::Value* json,
                                     const ErrorCallback error_callback,
                                     base::DictionaryValue** dictionary) {
  base::DictionaryValue* response_dictionary = NULL;

  if (!json || !json->GetAsDictionary(&response_dictionary)) {
    error_callback.Run("No JSON dictionary response received.");
    return false;
  }

  bool success = false;
  if (!response_dictionary->GetBoolean("success", &success)) {
    error_callback.Run("Cannot parse success state.");
    return false;
  }

  if (!success) {
    std::string message;
    response_dictionary->GetString("message", &message);
    error_callback.Run(message);
    return false;
  }

  *dictionary = response_dictionary;
  return true;
}

bool ParseRegisterStartResponse(const std::string& response,
                                const ErrorCallback error_callback,
                                std::string* polling_url_result,
                                std::string* registration_token_result,
                                std::string* complete_invite_url_result,
                                std::string* device_id_result) {
  scoped_ptr<base::Value> json(base::JSONReader::Read(response));
  base::DictionaryValue* response_dictionary = NULL;
  if (!GetJsonDictinaryAndCheckSuccess(json.get(), error_callback,
                                       &response_dictionary)) {
    return false;
  }

  std::string registration_token;
  if (!response_dictionary->GetString("registration_token",
                                      &registration_token)) {
    error_callback.Run("No registration_token specified.");
    return false;
  }

  std::string complete_invite_url;
  if (!response_dictionary->GetString("complete_invite_url",
                                      &complete_invite_url)) {
    error_callback.Run("No complete_invite_url specified.");
    return false;
  }

  std::string polling_url;
  if (!response_dictionary->GetString("polling_url", &polling_url)) {
    error_callback.Run("No polling_url specified.");
    return false;
  }

  base::ListValue* list = NULL;
  if (!response_dictionary->GetList("printers", &list)) {
    error_callback.Run("No printers list specified.");
    return false;
  }

  base::Value* printer_value = NULL;
  if (!list->Get(0, &printer_value)) {
    error_callback.Run("Printers list is empty.");
    return false;
  }

  base::DictionaryValue* printer = NULL;
  if (!printer_value->GetAsDictionary(&printer)) {
    error_callback.Run("Printer is not a json-dictionary.");
    return false;
  }

  std::string device_id;
  if (!printer->GetString("id", &device_id)) {
    error_callback.Run("No id specified.");
    return false;
  }

  if (device_id.empty()) {
    error_callback.Run("id is empty.");
    return false;
  }

  *polling_url_result = polling_url;
  *registration_token_result = registration_token;
  *complete_invite_url_result = complete_invite_url;
  *device_id_result = device_id;
  return true;
}

bool ParseRegisterCompleteResponse(const std::string& response,
                                   const ErrorCallback error_callback,
                                   std::string* authorization_code_result) {
  scoped_ptr<base::Value> json(base::JSONReader::Read(response));
  base::DictionaryValue* response_dictionary = NULL;
  if (!GetJsonDictinaryAndCheckSuccess(json.get(), error_callback,
                                       &response_dictionary)) {
    return false;
  }

  std::string authorization_code;
  if (!response_dictionary->GetString("authorization_code",
                                      &authorization_code)) {
    error_callback.Run("Cannot parse authorization_code.");
    return false;
  }

  *authorization_code_result = authorization_code;
  return true;
}

}  // namespace cloud_print_response_parser


// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

#include "base/json/json_string_value_serializer.h"
#include "base/values.h"

namespace ntp_snippets {

NTPSnippetsService::NTPSnippetsService(
    const std::string& application_language_code)
    : loaded_(false), application_language_code_(application_language_code) {}

NTPSnippetsService::~NTPSnippetsService() {}

void NTPSnippetsService::Shutdown() {
  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceShutdown(this));
  loaded_ = false;
}

void NTPSnippetsService::AddObserver(NTPSnippetsServiceObserver* observer) {
  observers_.AddObserver(observer);
  if (loaded_)
    observer->NTPSnippetsServiceLoaded(this);
}

void NTPSnippetsService::RemoveObserver(NTPSnippetsServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool NTPSnippetsService::LoadFromJSONString(const std::string& str) {
  JSONStringValueDeserializer deserializer(str);
  int error_code;
  std::string error_message;

  scoped_ptr<base::Value> deserialized =
      deserializer.Deserialize(&error_code, &error_message);
  if (!deserialized)
    return false;

  const base::DictionaryValue* top_dict = NULL;
  if (!deserialized->GetAsDictionary(&top_dict))
    return false;

  const base::ListValue* list = NULL;
  if (!top_dict->GetList("recos", &list))
    return false;

  for (base::Value* const value : *list) {
    const base::DictionaryValue* dict = NULL;
    if (!value->GetAsDictionary(&dict))
      return false;

    const base::DictionaryValue* content = NULL;
    if (!dict->GetDictionary("contentInfo", &content))
      return false;
    std::unique_ptr<NTPSnippet> snippet =
        NTPSnippet::NTPSnippetFromDictionary(*content);
    if (!snippet)
      return false;
    snippets_.push_back(std::move(snippet));
  }
  loaded_ = true;
  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceLoaded(this));
  return true;
}

}  // namespace ntp_snippets

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/declarative_api.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/api/events.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/permissions/permissions_data.h"

using extensions::core_api::events::Rule;

namespace AddRules = extensions::core_api::events::Event::AddRules;
namespace GetRules = extensions::core_api::events::Event::GetRules;
namespace RemoveRules = extensions::core_api::events::Event::RemoveRules;


namespace extensions {

namespace {

const char kWebRequest[] = "declarativeWebRequest.";
const char kWebViewExpectedError[] = "Webview event with Webview ID expected.";

bool IsWebViewEvent(const std::string& event_name) {
  // Sample event names:
  // webViewInternal.onRequest.
  // webViewInternal.onMessage.
  return event_name.compare(0,
                            strlen(webview::kWebViewEventPrefix),
                            webview::kWebViewEventPrefix) == 0;
}

std::string GetWebRequestEventName(const std::string& event_name) {
  std::string web_request_event_name(event_name);
  if (IsWebViewEvent(web_request_event_name)) {
    web_request_event_name.replace(
        0, strlen(webview::kWebViewEventPrefix), kWebRequest);
  }
  return web_request_event_name;
}

void ConvertBinaryDictionaryValuesToBase64(base::DictionaryValue* dict);

// Encodes |binary| as base64 and returns a new StringValue populated with the
// encoded string.
scoped_ptr<base::StringValue> ConvertBinaryToBase64(base::BinaryValue* binary) {
  std::string binary_data = std::string(binary->GetBuffer(), binary->GetSize());
  std::string data64;
  base::Base64Encode(binary_data, &data64);
  return scoped_ptr<base::StringValue>(new base::StringValue(data64));
}

// Parses through |args| replacing any BinaryValues with base64 encoded
// StringValues. Recurses over any nested ListValues, and calls
// ConvertBinaryDictionaryValuesToBase64 for any nested DictionaryValues.
void ConvertBinaryListElementsToBase64(base::ListValue* args) {
  size_t index = 0;
  for (base::ListValue::iterator iter = args->begin(); iter != args->end();
       ++iter, ++index) {
    if ((*iter)->IsType(base::Value::TYPE_BINARY)) {
      base::BinaryValue* binary = NULL;
      if (args->GetBinary(index, &binary))
        args->Set(index, ConvertBinaryToBase64(binary).release());
    } else if ((*iter)->IsType(base::Value::TYPE_LIST)) {
      base::ListValue* list;
      (*iter)->GetAsList(&list);
      ConvertBinaryListElementsToBase64(list);
    } else if ((*iter)->IsType(base::Value::TYPE_DICTIONARY)) {
      base::DictionaryValue* dict;
      (*iter)->GetAsDictionary(&dict);
      ConvertBinaryDictionaryValuesToBase64(dict);
    }
  }
}

// Parses through |dict| replacing any BinaryValues with base64 encoded
// StringValues. Recurses over any nested DictionaryValues, and calls
// ConvertBinaryListElementsToBase64 for any nested ListValues.
void ConvertBinaryDictionaryValuesToBase64(base::DictionaryValue* dict) {
  for (base::DictionaryValue::Iterator iter(*dict); !iter.IsAtEnd();
       iter.Advance()) {
    if (iter.value().IsType(base::Value::TYPE_BINARY)) {
      base::BinaryValue* binary = NULL;
      if (dict->GetBinary(iter.key(), &binary))
        dict->Set(iter.key(), ConvertBinaryToBase64(binary).release());
    } else if (iter.value().IsType(base::Value::TYPE_LIST)) {
      const base::ListValue* list;
      iter.value().GetAsList(&list);
      ConvertBinaryListElementsToBase64(const_cast<base::ListValue*>(list));
    } else if (iter.value().IsType(base::Value::TYPE_DICTIONARY)) {
      const base::DictionaryValue* dict;
      iter.value().GetAsDictionary(&dict);
      ConvertBinaryDictionaryValuesToBase64(
          const_cast<base::DictionaryValue*>(dict));
    }
  }
}

}  // namespace

RulesFunction::RulesFunction()
    : rules_registry_(NULL) {
}

RulesFunction::~RulesFunction() {}

bool RulesFunction::HasPermission() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));
  if (IsWebViewEvent(event_name) &&
      extension_->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kWebView))
    return true;
  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          event_name,
          extension_.get(),
          Feature::BLESSED_EXTENSION_CONTEXT,
          source_url());
  return availability.is_available();
}

bool RulesFunction::RunAsync() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  int webview_instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &webview_instance_id));
  int embedder_process_id = render_view_host()->GetProcess()->GetID();

  bool has_webview = webview_instance_id != 0;
  if (has_webview != IsWebViewEvent(event_name))
    EXTENSION_FUNCTION_ERROR(kWebViewExpectedError);
  event_name = GetWebRequestEventName(event_name);

  // If we are not operating on a particular <webview>, then the key is (0, 0).
  RulesRegistry::WebViewKey key(
      webview_instance_id ? embedder_process_id : 0, webview_instance_id);

  // The following call will return a NULL pointer for apps_shell, but should
  // never be called there anyways.
  rules_registry_ = ExtensionsAPIClient::Get()->GetRulesRegistry(
      browser_context(), key, event_name);
  DCHECK(rules_registry_.get());
  // Raw access to this function is not available to extensions, therefore
  // there should never be a request for a nonexisting rules registry.
  EXTENSION_FUNCTION_VALIDATE(rules_registry_.get());

  if (content::BrowserThread::CurrentlyOn(rules_registry_->owner_thread())) {
    bool success = RunAsyncOnCorrectThread();
    SendResponse(success);
  } else {
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
        content::BrowserThread::GetMessageLoopProxyForThread(
            rules_registry_->owner_thread());
    base::PostTaskAndReplyWithResult(
        message_loop_proxy.get(),
        FROM_HERE,
        base::Bind(&RulesFunction::RunAsyncOnCorrectThread, this),
        base::Bind(&RulesFunction::SendResponse, this));
  }

  return true;
}

bool EventsEventAddRulesFunction::RunAsyncOnCorrectThread() {
  ConvertBinaryListElementsToBase64(args_.get());
  scoped_ptr<AddRules::Params> params(AddRules::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  error_ = rules_registry_->AddRules(extension_id(), params->rules);

  if (error_.empty())
    results_ = AddRules::Results::Create(params->rules);

  return error_.empty();
}

bool EventsEventRemoveRulesFunction::RunAsyncOnCorrectThread() {
  scoped_ptr<RemoveRules::Params> params(RemoveRules::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->rule_identifiers.get()) {
    error_ = rules_registry_->RemoveRules(extension_id(),
                                          *params->rule_identifiers);
  } else {
    error_ = rules_registry_->RemoveAllRules(extension_id());
  }

  return error_.empty();
}

bool EventsEventGetRulesFunction::RunAsyncOnCorrectThread() {
  scoped_ptr<GetRules::Params> params(GetRules::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<linked_ptr<Rule> > rules;
  if (params->rule_identifiers.get()) {
    rules_registry_->GetRules(
        extension_id(), *params->rule_identifiers, &rules);
  } else {
    rules_registry_->GetAllRules(extension_id(), &rules);
  }

  results_ = GetRules::Results::Create(rules);

  return true;
}

}  // namespace extensions

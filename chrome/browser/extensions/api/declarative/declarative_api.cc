// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/declarative_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/guest_view/web_view/web_view_constants.h"
#include "chrome/browser/guest_view/web_view/web_view_guest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/events.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/permissions/permissions_data.h"

using extensions::api::events::Rule;

namespace AddRules = extensions::api::events::Event::AddRules;
namespace GetRules = extensions::api::events::Event::GetRules;
namespace RemoveRules = extensions::api::events::Event::RemoveRules;


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
          event_name, extension_, Feature::BLESSED_EXTENSION_CONTEXT,
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
  RulesRegistryService::WebViewKey key(
      webview_instance_id ? embedder_process_id : 0, webview_instance_id);

  RulesRegistryService* rules_registry_service =
      RulesRegistryService::Get(GetProfile());
  rules_registry_ = rules_registry_service->GetRulesRegistry(key, event_name);
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

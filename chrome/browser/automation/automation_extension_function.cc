// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements AutomationExtensionFunction.

#include "chrome/browser/automation/automation_extension_function.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"

TabContents* AutomationExtensionFunction::api_handler_tab_ = NULL;
AutomationExtensionFunction::PendingFunctionsMap
    AutomationExtensionFunction::pending_functions_;

AutomationExtensionFunction::AutomationExtensionFunction() {
}

void AutomationExtensionFunction::SetArgs(const ListValue* args) {
  // Need to JSON-encode for sending over the wire to the automation user.
  base::JSONWriter::Write(args, false, &args_);
}

const std::string AutomationExtensionFunction::GetResult() {
  // Already JSON-encoded, so override the base class's implementation.
  return json_result_;
}

bool AutomationExtensionFunction::RunImpl() {
  namespace keys = extension_automation_constants;

  DCHECK(api_handler_tab_) <<
      "Why is this function still enabled if no target tab?";
  if (!api_handler_tab_) {
    error_ = "No longer automating functions.";
    return false;
  }

  // We are being driven through automation, so we send the extension API
  // request over to the automation host. We do this before decoding the
  // 'args' JSON, otherwise we'd be decoding it only to encode it again.
  DictionaryValue message_to_host;
  message_to_host.SetString(keys::kAutomationNameKey, name_);
  message_to_host.SetString(keys::kAutomationArgsKey, args_);
  message_to_host.SetInteger(keys::kAutomationRequestIdKey, request_id_);
  message_to_host.SetBoolean(keys::kAutomationHasCallbackKey, has_callback_);
  // Send the API request's associated tab along to the automation client, so
  // that it can determine the execution context of the API call.
  TabContents* contents = NULL;
  ExtensionFunctionDispatcher* function_dispatcher = dispatcher();
  if (function_dispatcher && function_dispatcher->delegate()) {
    contents = function_dispatcher->delegate()->associated_tab_contents();
  } else {
    NOTREACHED() << "Extension function dispatcher delegate not found.";
  }
  if (contents)
    message_to_host.Set(keys::kAutomationTabJsonKey,
                        ExtensionTabUtil::CreateTabValue(contents));

  std::string message;
  base::JSONWriter::Write(&message_to_host, false, &message);
  if (api_handler_tab_->delegate()) {
    api_handler_tab_->delegate()->ForwardMessageToExternalHost(
        message, keys::kAutomationOrigin, keys::kAutomationRequestTarget);
  } else {
    NOTREACHED() << "ExternalTabContainer is supposed to correctly manage "
        "lifetime of api_handler_tab_.";
  }

  // Automation APIs are asynchronous so we need to stick around until
  // our response comes back.  Add ourselves to a static hash map keyed
  // by request ID.  The hash map keeps a reference count on us.
  DCHECK(pending_functions_.find(request_id_) == pending_functions_.end());
  pending_functions_[request_id_] = this;

  return true;
}

ExtensionFunction* AutomationExtensionFunction::Factory() {
  return new AutomationExtensionFunction();
}

void AutomationExtensionFunction::Enable(
    TabContents* api_handler_tab,
    const std::vector<std::string>& functions_enabled) {
  DCHECK(api_handler_tab);
  if (api_handler_tab_ && api_handler_tab != api_handler_tab_) {
    NOTREACHED() << "Don't call with different API handler.";
    return;
  }
  api_handler_tab_ = api_handler_tab;

  std::vector<std::string> function_names;
  if (functions_enabled.size() == 1 && functions_enabled[0] == "*") {
    ExtensionFunctionDispatcher::GetAllFunctionNames(&function_names);
  } else {
    function_names = functions_enabled;
  }

  for (std::vector<std::string>::iterator it = function_names.begin();
       it != function_names.end(); it++) {
    // TODO(joi) Could make this a per-profile change rather than a global
    // change. Could e.g. have the AutomationExtensionFunction store the
    // profile pointer and dispatch to the original ExtensionFunction when the
    // current profile is not that.
    bool result = ExtensionFunctionDispatcher::OverrideFunction(
        *it, AutomationExtensionFunction::Factory);
    LOG_IF(WARNING, !result) << "Failed to override API function: " << *it;
  }
}

void AutomationExtensionFunction::Disable() {
  api_handler_tab_ = NULL;
  ExtensionFunctionDispatcher::ResetFunctions();
}

bool AutomationExtensionFunction::InterceptMessageFromExternalHost(
    RenderViewHost* view_host,
    const std::string& message,
    const std::string& origin,
    const std::string& target) {
  namespace keys = extension_automation_constants;

  // We want only specially-tagged messages passed via the conduit tab.
  if (api_handler_tab_ &&
      view_host == api_handler_tab_->render_view_host() &&
      origin == keys::kAutomationOrigin &&
      target == keys::kAutomationResponseTarget) {
    // This is an extension API response being sent back via postMessage,
    // so redirect it.
    scoped_ptr<Value> message_value(base::JSONReader::Read(message, false));
    DCHECK(message_value->IsType(Value::TYPE_DICTIONARY));
    if (message_value->IsType(Value::TYPE_DICTIONARY)) {
      DictionaryValue* message_dict =
          reinterpret_cast<DictionaryValue*>(message_value.get());

      int request_id = -1;
      bool got_value = message_dict->GetInteger(keys::kAutomationRequestIdKey,
                                                &request_id);
      DCHECK(got_value);
      if (got_value) {
        std::string error;
        bool success = !message_dict->GetString(keys::kAutomationErrorKey,
                                                &error);

        std::string response;
        got_value = message_dict->GetString(keys::kAutomationResponseKey,
                                            &response);
        DCHECK(!success || got_value);

        PendingFunctionsMap::iterator it = pending_functions_.find(request_id);
        DCHECK(it != pending_functions_.end());

        if (it != pending_functions_.end()) {
          scoped_refptr<AutomationExtensionFunction> func = it->second;
          pending_functions_.erase(it);

          // Our local ref should be the last remaining.
          DCHECK(func && func->HasOneRef());

          if (func) {
            func->json_result_ = response;
            func->error_ = error;

            func->SendResponse(success);
          }
        }
        return true;
      }
    }
  }

  return false;
}

AutomationExtensionFunction::~AutomationExtensionFunction() {
}

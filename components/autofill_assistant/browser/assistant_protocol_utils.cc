// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_protocol_utils.h"

#include "base/logging.h"
#include "components/autofill_assistant/browser/actions/assistant_click_action.h"
#include "components/autofill_assistant/browser/actions/assistant_tell_action.h"
#include "components/autofill_assistant/browser/actions/assistant_use_address_action.h"
#include "components/autofill_assistant/browser/actions/assistant_use_card_action.h"
#include "components/autofill_assistant/browser/actions/assistant_wait_for_dom_action.h"
#include "components/autofill_assistant/browser/assistant.pb.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

namespace autofill_assistant {

// static
std::string AssistantProtocolUtils::CreateGetScriptsRequest(const GURL& url) {
  DCHECK(!url.is_empty());

  SupportsScriptRequestProto script_proto;
  script_proto.set_url(url.spec());
  script_proto.mutable_client_context()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());
  std::string serialized_script_proto;
  bool success = script_proto.SerializeToString(&serialized_script_proto);
  DCHECK(success);
  return serialized_script_proto;
}

// static
bool AssistantProtocolUtils::ParseAssistantScripts(
    const std::string& response,
    std::map<AssistantScript*, std::unique_ptr<AssistantScript>>*
        assistant_scripts) {
  DCHECK(!response.empty());
  DCHECK(assistant_scripts);

  SupportsScriptResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse getting assistant scripts response.";
    return false;
  }

  for (const auto& script : response_proto.scripts()) {
    auto assistant_script = std::make_unique<AssistantScript>();
    assistant_script->path = script.path();

    if (script.has_presentation()) {
      const auto& presentation = script.presentation();
      if (presentation.has_name())
        assistant_script->name = presentation.name();

      if (presentation.has_precondition()) {
        std::vector<std::vector<std::string>> elements_exist;
        for (const auto& element :
             presentation.precondition().elements_exist()) {
          std::vector<std::string> selectors;
          for (const auto& selector : element.selectors()) {
            selectors.emplace_back(selector);
          }
          elements_exist.emplace_back(selectors);
        }

        if (!elements_exist.empty()) {
          assistant_script->precondition =
              std::make_unique<AssistantScriptPrecondition>(elements_exist);
        }
      }
    }

    (*assistant_scripts)[assistant_script.get()] = std::move(assistant_script);
  }

  return true;
}

// static
std::string AssistantProtocolUtils::CreateInitialScriptActionsRequest(
    const std::string& script_path) {
  InitialScriptActionsRequestProto::QueryProto query;
  query.set_script_path(script_path);
  query.set_policy(PolicyType::SCRIPT);

  InitialScriptActionsRequestProto initial_request_proto;
  initial_request_proto.set_allocated_query(&query);

  std::string serialized_initial_request_proto;
  bool success = initial_request_proto.SerializeToString(
      &serialized_initial_request_proto);
  DCHECK(success);
  return serialized_initial_request_proto;
}

// static
std::string AssistantProtocolUtils::CreateNextScriptActionsRequest(
    const std::string& previous_server_payload) {
  NextScriptActionsRequestProto request_proto;
  request_proto.set_server_payload(previous_server_payload);

  std::string serialized_request_proto;
  bool success = request_proto.SerializeToString(&serialized_request_proto);
  DCHECK(success);
  return serialized_request_proto;
}

// static
bool AssistantProtocolUtils::ParseAssistantActions(
    const std::string& response,
    std::string* return_server_payload,
    std::vector<std::unique_ptr<AssistantAction>>* assistant_actions) {
  DCHECK(!response.empty());
  DCHECK(assistant_actions);

  ActionsResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse assistant actions response.";
    return false;
  }

  if (return_server_payload && response_proto.has_server_payload()) {
    *return_server_payload = response_proto.server_payload();
  }

  for (const auto& action : response_proto.actions()) {
    switch (action.action_info_case()) {
      case AssistantActionProto::ActionInfoCase::kClick: {
        DCHECK(action.has_click());
        std::vector<std::string> selectors;
        for (const auto& selector :
             action.click().element_to_click().selectors()) {
          selectors.emplace_back(selector);
        }
        DCHECK(!selectors.empty());
        assistant_actions->emplace_back(
            std::make_unique<AssistantClickAction>(selectors));
        break;
      }
      case AssistantActionProto::ActionInfoCase::kTell: {
        DCHECK(action.has_tell());
        assistant_actions->emplace_back(
            std::make_unique<AssistantTellAction>(action.tell().message()));
        break;
      }
      case AssistantActionProto::ActionInfoCase::kUseAddress: {
        DCHECK(action.has_use_address());
        std::vector<std::string> selectors;
        for (const auto& selector :
             action.use_address().form_field_element().selectors()) {
          selectors.emplace_back(selector);
        }
        DCHECK(!selectors.empty());
        assistant_actions->emplace_back(
            std::make_unique<AssistantUseAddressAction>(
                action.use_address().has_usage() ? action.use_address().usage()
                                                 : "",
                selectors));
        break;
      }
      case AssistantActionProto::ActionInfoCase::kUseCard: {
        DCHECK(action.has_use_card());
        std::vector<std::string> selectors;
        for (const auto& selector :
             action.use_card().form_field_element().selectors()) {
          selectors.emplace_back(selector);
        }
        DCHECK(!selectors.empty());
        assistant_actions->emplace_back(
            std::make_unique<AssistantUseCardAction>(selectors));
        break;
      }
      case AssistantActionProto::ActionInfoCase::kWaitForDom: {
        DCHECK(action.has_wait_for_dom());
        std::vector<std::string> selectors;
        for (const auto& selector :
             action.wait_for_dom().element().selectors()) {
          selectors.emplace_back(selector);
        }
        DCHECK(!selectors.empty());
        assistant_actions->emplace_back(
            std::make_unique<AssistantWaitForDomAction>(
                action.wait_for_dom().has_timeout_ms()
                    ? action.wait_for_dom().timeout_ms()
                    : 0,
                selectors,
                action.wait_for_dom().has_check_for_absence() &&
                    action.wait_for_dom().check_for_absence()));
        break;
      }
      case AssistantActionProto::ActionInfoCase::ACTION_INFO_NOT_SET: {
        LOG(ERROR) << "Unknown or unspported assistant action.";
        break;
      }
    }
  }

  return true;
}

}  // namespace autofill_assistant

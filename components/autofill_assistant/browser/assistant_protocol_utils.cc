// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_protocol_utils.h"

#include "base/logging.h"
#include "components/autofill_assistant/browser/actions/assistant_click_action.h"
#include "components/autofill_assistant/browser/actions/assistant_tell_action.h"
#include "components/autofill_assistant/browser/assistant.pb.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

namespace autofill_assistant {

// static
std::string AssistantProtocolUtils::CreateGetScriptsRequest(const GURL& url) {
  DCHECK(!url.is_empty());
  ClientContextProto context_proto;
  context_proto.set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());

  SupportsScriptRequestProto script_proto;
  script_proto.set_url(url.spec());
  script_proto.set_allocated_client_context(&context_proto);

  std::string serialized_script_proto;
  bool success = script_proto.SerializeToString(&serialized_script_proto);
  DCHECK(success);
  return serialized_script_proto;
}

// static
AssistantProtocolUtils::AssistantScripts
AssistantProtocolUtils::ParseAssistantScripts(const std::string& response) {
  DCHECK(!response.empty());
  AssistantScripts scripts;

  SupportsScriptResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse getting assistant scripts response.";
    return scripts;
  }

  for (const auto& script : response_proto.scripts()) {
    std::string name = "";
    if (script.has_presentation() && script.presentation().has_name()) {
      name = script.presentation().name();
    }
    auto assistant_script = std::make_unique<AssistantScript>();
    assistant_script->name = name;
    assistant_script->path = script.path();
    scripts[assistant_script.get()] = std::move(assistant_script);
  }

  return scripts;
}

// static
std::string AssistantProtocolUtils::CreateInitialScriptActionRequest(
    const std::string& script_path) {
  InitialScriptActionRequestProto::QueryProto query;
  query.set_script_path(script_path);
  query.set_policy(PolicyType::SCRIPT);

  InitialScriptActionRequestProto initial_request_proto;
  initial_request_proto.set_allocated_query(&query);

  std::string serialized_initial_request_proto;
  bool success = initial_request_proto.SerializeToString(
      &serialized_initial_request_proto);
  DCHECK(success);
  return serialized_initial_request_proto;
}

// static
AssistantProtocolUtils::AssistantActions
AssistantProtocolUtils::ParseAssistantActions(
    const std::string& response,
    std::string* return_server_payload) {
  DCHECK(!response.empty());
  AssistantActions actions;

  ActionsResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse assistant actions response.";
    return actions;
  }

  if (return_server_payload && response_proto.has_server_payload()) {
    *return_server_payload = response_proto.server_payload();
  }

  for (const auto& action : response_proto.actions()) {
    switch (action.action_info_case()) {
      case AssistantActionProto::ActionInfoCase::kClickSpecification: {
        DCHECK(action.has_click_specification());
        std::vector<std::string> selectors;
        for (const auto& selector :
             action.click_specification().element_to_click().selectors()) {
          selectors.emplace_back(selector);
        }
        DCHECK(!selectors.empty());
        actions.emplace_back(std::make_unique<AssistantClickAction>(selectors));
        break;
      }
      case AssistantActionProto::ActionInfoCase::kTellSpecification: {
        DCHECK(action.has_tell_specification());
        actions.emplace_back(std::make_unique<AssistantTellAction>(
            action.tell_specification().message()));
        break;
      }
      case AssistantActionProto::ActionInfoCase::ACTION_INFO_NOT_SET: {
        LOG(ERROR) << "Unknown or unspported assistant action.";
        break;
      }
    }
  }

  return actions;
}

}  // namespace autofill_assistant

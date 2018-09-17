// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/protocol_utils.h"

#include <utility>

#include "base/logging.h"
#include "components/autofill_assistant/browser/actions/autofill_action.h"
#include "components/autofill_assistant/browser/actions/click_action.h"
#include "components/autofill_assistant/browser/actions/focus_element_action.h"
#include "components/autofill_assistant/browser/actions/select_option_action.h"
#include "components/autofill_assistant/browser/actions/tell_action.h"
#include "components/autofill_assistant/browser/actions/upload_dom_action.h"
#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

namespace autofill_assistant {

// static
std::string ProtocolUtils::CreateGetScriptsRequest(const GURL& url) {
  DCHECK(!url.is_empty());

  SupportsScriptRequestProto script_proto;
  script_proto.set_url(url.spec());
  script_proto.mutable_client_context()->mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());
  std::string serialized_script_proto;
  bool success = script_proto.SerializeToString(&serialized_script_proto);
  DCHECK(success);
  return serialized_script_proto;
}

// static
bool ProtocolUtils::ParseScripts(
    const std::string& response,
    std::vector<std::unique_ptr<Script>>* scripts) {
  DCHECK(scripts);

  SupportsScriptResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse getting assistant scripts response.";
    return false;
  }

  scripts->clear();
  for (const auto& script_proto : response_proto.scripts()) {
    auto script = std::make_unique<Script>();
    script->handle.path = script_proto.path();

    const auto& presentation = script_proto.presentation();
    script->handle.name = presentation.name();
    script->precondition =
        ScriptPrecondition::FromProto(presentation.precondition());

    if (script->handle.name.empty() || script->handle.path.empty() ||
        !script->precondition) {
      LOG(ERROR) << "Ignored invalid or incomplete script '"
                 << script->handle.path << "'";
      continue;
    }
    scripts->emplace_back(std::move(script));
  }

  return true;
}

// static
std::string ProtocolUtils::CreateInitialScriptActionsRequest(
    const std::string& script_path) {
  ScriptActionRequestProto request_proto;
  InitialScriptActionsRequestProto::QueryProto* query =
      request_proto.mutable_initial_request()->mutable_query();
  query->add_script_path(script_path);
  query->set_policy(PolicyType::SCRIPT);
  request_proto.mutable_client_context()->mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());

  std::string serialized_initial_request_proto;
  bool success =
      request_proto.SerializeToString(&serialized_initial_request_proto);
  DCHECK(success);
  return serialized_initial_request_proto;
}

// static
std::string ProtocolUtils::CreateNextScriptActionsRequest(
    const std::string& previous_server_payload,
    const std::vector<ProcessedActionProto>& processed_actions) {
  ScriptActionRequestProto request_proto;
  request_proto.set_server_payload(previous_server_payload);
  NextScriptActionsRequestProto* next_request =
      request_proto.mutable_next_request();
  for (const auto& processed_action : processed_actions) {
    next_request->add_processed_actions()->MergeFrom(processed_action);
  }
  std::string serialized_request_proto;
  bool success = request_proto.SerializeToString(&serialized_request_proto);
  DCHECK(success);
  return serialized_request_proto;
}

// static
bool ProtocolUtils::ParseActions(const std::string& response,
                                 std::string* return_server_payload,
                                 std::deque<std::unique_ptr<Action>>* actions) {
  DCHECK(actions);

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
      case ActionProto::ActionInfoCase::kClick: {
        actions->emplace_back(std::make_unique<ClickAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kTell: {
        actions->emplace_back(std::make_unique<TellAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kFocusElement: {
        actions->emplace_back(std::make_unique<FocusElementAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kUseAddress:
      case ActionProto::ActionInfoCase::kUseCard: {
        actions->emplace_back(std::make_unique<AutofillAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kWaitForDom: {
        actions->emplace_back(std::make_unique<WaitForDomAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kUploadDom: {
        actions->emplace_back(std::make_unique<UploadDomAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kSelectOption: {
        actions->emplace_back(std::make_unique<SelectOptionAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::ACTION_INFO_NOT_SET: {
        LOG(ERROR) << "Unknown or unspported action.";
        break;
      }
    }
  }

  return true;
}

}  // namespace autofill_assistant

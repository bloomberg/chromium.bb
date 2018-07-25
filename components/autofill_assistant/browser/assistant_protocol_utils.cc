// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_protocol_utils.h"

#include "base/logging.h"
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

}  // namespace autofill_assistant

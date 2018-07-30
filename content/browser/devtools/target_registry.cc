// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/target_registry.h"

#include "base/strings/stringprintf.h"
#include "content/browser/devtools/devtools_session.h"

namespace content {

namespace {
static const char kMethod[] = "method";
static const char kResumeMethod[] = "Runtime.runIfWaitingForDebugger";
}  // namespace

// static
bool TargetRegistry::IsRuntimeResumeCommand(base::Value* value) {
  if (value && value->is_dict()) {
    base::Value* method = value->FindKey(kMethod);
    return method && method->is_string() &&
           method->GetString() == kResumeMethod;
  }
  return false;
}

struct TargetRegistry::SessionInfo {
  SessionInfo(scoped_refptr<DevToolsAgentHostImpl> agent_host,
              DevToolsAgentHostClient* client,
              base::OnceClosure resume_if_throttled)
      : agent_host(agent_host),
        client(client),
        resume_if_throttled(std::move(resume_if_throttled)) {}
  scoped_refptr<DevToolsAgentHostImpl> agent_host;
  DevToolsAgentHostClient* client;
  base::OnceClosure resume_if_throttled;
};

TargetRegistry::TargetRegistry(DevToolsSession* root_session)
    : root_session_(root_session) {}

TargetRegistry::~TargetRegistry() {}

void TargetRegistry::AttachSubtargetSession(
    const std::string& session_id,
    DevToolsAgentHostImpl* agent_host,
    DevToolsAgentHostClient* client,
    base::OnceClosure resume_if_throttled) {
  sessions_[session_id] = std::make_unique<SessionInfo>(
      agent_host, client, std::move(resume_if_throttled));
  agent_host->AttachSubtargetClient(client, this);
}
void TargetRegistry::DetachSubtargetSession(const std::string& session_id) {
  sessions_.erase(session_id);
}

bool TargetRegistry::CanDispatchMessageOnAgentHost(
    base::DictionaryValue* parsed_message) {
  return parsed_message->FindKeyOfType("sessionId",
                                       base::DictionaryValue::Type::STRING);
}

void TargetRegistry::DispatchMessageOnAgentHost(
    const std::string& message,
    std::unique_ptr<base::DictionaryValue> parsed_message) {
  std::string session_id;
  bool result = parsed_message->GetString("sessionId", &session_id);
  DCHECK(result);
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    LOG(ERROR) << "Unknown session " << session_id;
    return;
  }
  scoped_refptr<DevToolsAgentHostImpl> agent_host = it->second->agent_host;
  DevToolsAgentHostClient* client = it->second->client;
  if (!it->second->resume_if_throttled.is_null() &&
      IsRuntimeResumeCommand(parsed_message.get())) {
    std::move(it->second->resume_if_throttled).Run();
  }
  agent_host->DispatchProtocolMessage(client, message,
                                      std::move(parsed_message));
}

void TargetRegistry::SendMessageToClient(const std::string& session_id,
                                         const std::string& message) {
  DCHECK(message[message.length() - 1] == '}');
  std::string suffix =
      base::StringPrintf(", \"sessionId\": \"%s\"}", session_id.c_str());
  std::string patched;
  patched.reserve(message.length() + suffix.length() - 1);
  patched.append(message.data(), message.length() - 1);
  patched.append(suffix);
  root_session_->client()->DispatchProtocolMessage(root_session_->agent_host(),
                                                   patched);
}

}  // namespace content

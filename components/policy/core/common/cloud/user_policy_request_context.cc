// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_policy_request_context.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request_context.h"

namespace policy {

UserPolicyRequestContext::UserPolicyRequestContext(
    scoped_refptr<net::URLRequestContextGetter> user_context_getter,
    scoped_refptr<net::URLRequestContextGetter> system_context_getter,
    const std::string& user_agent)
    : user_context_getter_(user_context_getter),
      system_context_getter_(system_context_getter),
      http_user_agent_settings_("*", user_agent) {
  DCHECK(user_context_getter_);
}

UserPolicyRequestContext::~UserPolicyRequestContext() {
}

net::URLRequestContext*
UserPolicyRequestContext::GetURLRequestContext() {
  DCHECK(GetNetworkTaskRunner()->RunsTasksOnCurrentThread());
  if (!context_.get()) {
    // Create our URLRequestContext().
    context_.reset(new net::URLRequestContext());
    net::URLRequestContext* user_context =
        user_context_getter_->GetURLRequestContext();

    // Reuse pretty much everything from the user context, except we
    // use the system context's proxy and resolver (see below).
    context_->CopyFrom(user_context);

    // Use the system context's proxy and resolver to ensure that we can still
    // fetch policy updates even if a bad proxy config is pushed via user
    // policy.
    // TODO(atwilson): Re-enable the following lines in a followup CL per
    // reviewer request.
    // net::URLRequestContext* system_context =
    //        system_context_getter_->GetURLRequestContext();
    // context_->set_host_resolver(system_context->host_resolver());
    // context_->set_proxy_service(system_context->proxy_service());

    // Set our custom UserAgent.
    context_->set_http_user_agent_settings(&http_user_agent_settings_);
  }
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
UserPolicyRequestContext::GetNetworkTaskRunner() const {
  return user_context_getter_->GetNetworkTaskRunner();
}

}  // namespace policy

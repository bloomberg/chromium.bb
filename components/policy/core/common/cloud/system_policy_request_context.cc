// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/system_policy_request_context.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request_context.h"

namespace policy {

SystemPolicyRequestContext::SystemPolicyRequestContext(
    scoped_refptr<net::URLRequestContextGetter> system_context_getter,
    const std::string& user_agent)
    : system_context_getter_(system_context_getter),
      http_user_agent_settings_("*", user_agent) {
  DCHECK(system_context_getter);
}

SystemPolicyRequestContext::~SystemPolicyRequestContext() {
}

net::URLRequestContext*
SystemPolicyRequestContext::GetURLRequestContext() {
  DCHECK(GetNetworkTaskRunner()->RunsTasksOnCurrentThread());
  if (!context_.get()) {
    // Create our URLRequestContext().
    context_.reset(new net::URLRequestContext());

    net::URLRequestContext* system_context =
        system_context_getter_->GetURLRequestContext();
    // Share resolver, proxy service and ssl bits with the system context.
    // This is important so we don't make redundant requests (e.g. when
    // resolving proxy auto configuration).
    // TODO(atwilson): Consider using CopyFrom() here to copy all services -
    // http://crbug.com/322422.
    context_->set_net_log(system_context->net_log());
    context_->set_host_resolver(system_context->host_resolver());
    context_->set_proxy_service(system_context->proxy_service());
    context_->set_ssl_config_service(
        system_context->ssl_config_service());

    // Share the job factory (if there is one). This allows tests to intercept
    // requests made via this request context if they install protocol handlers
    // at the system request context.
    context_->set_job_factory(system_context->job_factory());

    // Set our custom UserAgent.
    context_->set_http_user_agent_settings(&http_user_agent_settings_);

    // Share the http session.
    http_transaction_factory_.reset(new net::HttpNetworkLayer(
        system_context->http_transaction_factory()->GetSession()));
    context_->set_http_transaction_factory(http_transaction_factory_.get());

    // No cookies, please. We also don't track channel IDs (no
    // ServerBoundCertService).
    context_->set_cookie_store(new net::CookieMonster(NULL, NULL));
  }

  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemPolicyRequestContext::GetNetworkTaskRunner() const {
  return system_context_getter_->GetNetworkTaskRunner();
}

}  // namespace policy

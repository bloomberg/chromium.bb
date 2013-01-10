// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/declarative_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/events.h"
#include "content/public/browser/browser_thread.h"

using extensions::api::events::Rule;

namespace AddRules = extensions::api::events::Event::AddRules;
namespace GetRules = extensions::api::events::Event::GetRules;
namespace RemoveRules = extensions::api::events::Event::RemoveRules;

namespace extensions {

RulesFunction::RulesFunction() : rules_registry_(NULL) {}

RulesFunction::~RulesFunction() {}

bool RulesFunction::HasPermission() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));
  return extension_->HasAPIPermission(event_name);
}

bool RulesFunction::RunImpl() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  RulesRegistryService* rules_registry_service =
      ExtensionSystemFactory::GetForProfile(profile())->
      rules_registry_service();
  rules_registry_ = rules_registry_service->GetRulesRegistry(event_name);
  // Raw access to this function is not available to extensions, therefore
  // there should never be a request for a nonexisting rules registry.
  EXTENSION_FUNCTION_VALIDATE(rules_registry_);

  if (content::BrowserThread::CurrentlyOn(rules_registry_->GetOwnerThread())) {
    bool success = RunImplOnCorrectThread();
    SendResponse(success);
  } else {
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
        content::BrowserThread::GetMessageLoopProxyForThread(
            rules_registry_->GetOwnerThread());
    base::PostTaskAndReplyWithResult(
        message_loop_proxy,
        FROM_HERE,
        base::Bind(&RulesFunction::RunImplOnCorrectThread, this),
        base::Bind(&RulesFunction::SendResponse, this));
  }

  return true;
}

bool AddRulesFunction::RunImplOnCorrectThread() {
  scoped_ptr<AddRules::Params> params(AddRules::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  error_ = rules_registry_->AddRules(extension_id(), params->rules);

  if (error_.empty())
    results_ = AddRules::Results::Create(params->rules);

  return error_.empty();
}

bool RemoveRulesFunction::RunImplOnCorrectThread() {
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

bool GetRulesFunction::RunImplOnCorrectThread() {
  scoped_ptr<GetRules::Params> params(GetRules::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<linked_ptr<Rule> > rules;
  if (params->rule_identifiers.get()) {
    error_ = rules_registry_->GetRules(extension_id(),
                                       *params->rule_identifiers,
                                       &rules);
  } else {
    error_ = rules_registry_->GetAllRules(extension_id(), &rules);
  }

  if (error_.empty())
    results_ = GetRules::Results::Create(rules);

  return error_.empty();
}

}  // namespace extensions

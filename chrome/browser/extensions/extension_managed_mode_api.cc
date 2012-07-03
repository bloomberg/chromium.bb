// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the Chrome Extensions Managed Mode API.

#include "chrome/browser/extensions/extension_managed_mode_api.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_preference_api_constants.h"
#include "chrome/browser/managed_mode.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/managed_mode_policy_provider_factory.h"
#endif

namespace {

// Event that is fired when we enter or leave managed mode.
const char kChangeEventName[] = "managedModePrivate.onChange";

// Key to report whether the attempt to enter managed mode succeeded.
const char kEnterSuccessKey[] = "success";

}  // namespace

namespace keys = extension_preference_api_constants;

ExtensionManagedModeEventRouter::ExtensionManagedModeEventRouter(
    Profile* profile) : profile_(profile) {
}

void ExtensionManagedModeEventRouter::Init() {
  registrar_.Init(g_browser_process->local_state());
  registrar_.Add(prefs::kInManagedMode, this);
}

ExtensionManagedModeEventRouter::~ExtensionManagedModeEventRouter() {
}

void ExtensionManagedModeEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PREF_CHANGED, type);
  const std::string& pref_name =
      *content::Details<std::string>(details).ptr();
  DCHECK_EQ(std::string(prefs::kInManagedMode), pref_name);

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  args.Append(dict);
  dict->SetBoolean(extension_preference_api_constants::kValue,
      g_browser_process->local_state()->GetBoolean(prefs::kInManagedMode));
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  ExtensionEventRouter* event_router = profile_->GetExtensionEventRouter();
  event_router->DispatchEventToRenderers(kChangeEventName, json_args,
                                         NULL, GURL());
}

GetManagedModeFunction::~GetManagedModeFunction() { }

bool GetManagedModeFunction::RunImpl() {
  bool in_managed_mode = ManagedMode::IsInManagedMode();

  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  result->SetBoolean(keys::kValue, in_managed_mode);
  result_.reset(result.release());
  return true;
}

EnterManagedModeFunction::~EnterManagedModeFunction() { }

bool EnterManagedModeFunction::RunImpl() {
  ManagedMode::EnterManagedMode(
      profile(),
      base::Bind(&EnterManagedModeFunction::SendResult, this));
  return true;
}

void EnterManagedModeFunction::SendResult(bool success) {
  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  result->SetBoolean(kEnterSuccessKey, success);
  result_.reset(result.release());
  SendResponse(true);
}

GetPolicyFunction::~GetPolicyFunction() { }

bool GetPolicyFunction::RunImpl() {
  std::string key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &key));
#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::ManagedModePolicyProvider* policy_provider =
      ManagedModePolicyProviderFactory::GetForProfile(profile_);
  const base::Value* policy = policy_provider->GetPolicy(key);
  if (policy)
    result_.reset(policy->DeepCopy());
#endif
  return true;
}

SetPolicyFunction::~SetPolicyFunction() { }

bool SetPolicyFunction::RunImpl() {
  std::string key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &key));
  base::Value* value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(1, &value));
#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::ManagedModePolicyProvider* policy_provider =
      ManagedModePolicyProviderFactory::GetForProfile(profile_);
  policy_provider->SetPolicy(key, value);
#endif
  return true;
}

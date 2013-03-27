// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the Chrome Extensions Managed Mode API.

#include "chrome/browser/extensions/api/managed_mode_private/managed_mode_private_api.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/preference/preference_api_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/managed_mode_private.h"
#include "chrome/common/extensions/api/managed_mode_private/managed_mode_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#endif

namespace {

// Event that is fired when we enter or leave managed mode.
const char kChangeEventName[] = "managedModePrivate.onChange";

// Key to report whether the attempt to enter managed mode succeeded.
const char kEnterSuccessKey[] = "success";

}  // namespace

namespace keys = extensions::preference_api_constants;

namespace extensions {

namespace GetPolicy = api::managed_mode_private::GetPolicy;
namespace SetPolicy = api::managed_mode_private::SetPolicy;

ManagedModeEventRouter::ManagedModeEventRouter(
    Profile* profile) : profile_(profile) {
  registrar_.Init(g_browser_process->local_state());
  registrar_.Add(
      prefs::kInManagedMode,
      base::Bind(&ManagedModeEventRouter::OnInManagedModeChanged,
                 base::Unretained(this)));
}

ManagedModeEventRouter::~ManagedModeEventRouter() {
}

void ManagedModeEventRouter::OnInManagedModeChanged() {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean(
      keys::kValue,
      g_browser_process->local_state()->GetBoolean(prefs::kInManagedMode));
  scoped_ptr<ListValue> args(new ListValue());
  args->Set(0, dict);

  extensions::EventRouter* event_router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  scoped_ptr<extensions::Event> event(new extensions::Event(
      kChangeEventName, args.Pass()));
  event_router->BroadcastEvent(event.Pass());
}

ManagedModePrivateGetFunction::~ManagedModePrivateGetFunction() { }

bool ManagedModePrivateGetFunction::RunImpl() {
  bool in_managed_mode = ManagedMode::IsInManagedMode();

  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  result->SetBoolean(keys::kValue, in_managed_mode);
  SetResult(result.release());
  return true;
}

ManagedModePrivateEnterFunction::~ManagedModePrivateEnterFunction() { }

bool ManagedModePrivateEnterFunction::RunImpl() {
  ManagedMode::EnterManagedMode(
      profile(),
      base::Bind(&ManagedModePrivateEnterFunction::SendResult, this));
  return true;
}

void ManagedModePrivateEnterFunction::SendResult(bool success) {
  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  result->SetBoolean(kEnterSuccessKey, success);
  SetResult(result.release());
  SendResponse(true);
}

ManagedModePrivateGetPolicyFunction::~ManagedModePrivateGetPolicyFunction() { }

bool ManagedModePrivateGetPolicyFunction::RunImpl() {
  scoped_ptr<GetPolicy::Params> params(GetPolicy::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::ManagedModePolicyProvider* policy_provider =
      profile_->GetManagedModePolicyProvider();
  const base::Value* policy = policy_provider->GetPolicy(params->key);
  if (policy)
    SetResult(policy->DeepCopy());
#endif
  return true;
}

ManagedModePrivateSetPolicyFunction::~ManagedModePrivateSetPolicyFunction() { }

bool ManagedModePrivateSetPolicyFunction::RunImpl() {
  scoped_ptr<SetPolicy::Params> params(SetPolicy::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::ManagedModePolicyProvider* policy_provider =
      profile_->GetManagedModePolicyProvider();
  policy_provider->SetPolicy(params->key, params->value.get());
#endif
  return true;
}

ManagedModeAPI::ManagedModeAPI(Profile* profile)
    : profile_(profile) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, kChangeEventName);
  (new ManagedModeHandler)->Register();
}

ManagedModeAPI::~ManagedModeAPI() {
}

void ManagedModeAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

static base::LazyInstance<ProfileKeyedAPIFactory<ManagedModeAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<ManagedModeAPI>* ManagedModeAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void ManagedModeAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  managed_mode_event_router_.reset(new ManagedModeEventRouter(profile_));
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

}  // namespace extensions

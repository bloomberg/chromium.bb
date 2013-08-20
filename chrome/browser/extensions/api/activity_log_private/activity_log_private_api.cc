// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"

#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/activity_log_private.h"
#include "chrome/common/pref_names.h"

namespace extensions {

namespace activity_log_private = api::activity_log_private;

using api::activity_log_private::ExtensionActivity;

const char kActivityLogExtensionId[] = "acldcpdepobcjbdanifkmfndkjoilgba";
const char kActivityLogTestExtensionId[] = "ajabfgledjhbabeoojlabelaifmakodf";

static base::LazyInstance<ProfileKeyedAPIFactory<ActivityLogAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<ActivityLogAPI>* ActivityLogAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

template<>
void ProfileKeyedAPIFactory<ActivityLogAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionSystemFactory::GetInstance());
  DependsOn(ActivityLogFactory::GetInstance());
}

ActivityLogAPI::ActivityLogAPI(Profile* profile)
    : profile_(profile),
      initialized_(false) {
  if (!ExtensionSystem::Get(profile_)->event_router()) {  // Check for testing.
    DVLOG(1) << "ExtensionSystem event_router does not exist.";
    return;
  }
  activity_log_ = extensions::ActivityLog::GetInstance(profile_);
  DCHECK(activity_log_);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, activity_log_private::OnExtensionActivity::kEventName);
  activity_log_->AddObserver(this);
  initialized_ = true;
}

ActivityLogAPI::~ActivityLogAPI() {
}

void ActivityLogAPI::Shutdown() {
  if (!initialized_) {  // Check for testing.
    DVLOG(1) << "ExtensionSystem event_router does not exist.";
    return;
  }
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
  activity_log_->RemoveObserver(this);
}

// static
bool ActivityLogAPI::IsExtensionWhitelisted(const std::string& extension_id) {
  return (extension_id == kActivityLogExtensionId ||
          extension_id == kActivityLogTestExtensionId);
}

void ActivityLogAPI::OnListenerAdded(const EventListenerInfo& details) {
  // TODO(felt): Only observe activity_log_ events when we have a customer.
}

void ActivityLogAPI::OnListenerRemoved(const EventListenerInfo& details) {
  // TODO(felt): Only observe activity_log_ events when we have a customer.
}

void ActivityLogAPI::OnExtensionActivity(scoped_refptr<Action> activity) {
  scoped_ptr<base::ListValue> value(new base::ListValue());
  scoped_ptr<ExtensionActivity> activity_arg =
      activity->ConvertToExtensionActivity();
  value->Append(activity_arg->ToValue().release());
  scoped_ptr<Event> event(
      new Event(activity_log_private::OnExtensionActivity::kEventName,
          value.Pass()));
  event->restrict_to_profile = profile_;
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(event.Pass());
}

bool ActivityLogPrivateGetExtensionActivitiesFunction::RunImpl() {
  return true;
}

}  // namespace extensions

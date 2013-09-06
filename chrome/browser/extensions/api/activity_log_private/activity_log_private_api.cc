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

using api::activity_log_private::ActivityResultSet;
using api::activity_log_private::ExtensionActivity;
using api::activity_log_private::Filter;

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
  scoped_ptr<activity_log_private::GetExtensionActivities::Params> params(
      activity_log_private::GetExtensionActivities::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Get the arguments in the right format.
  scoped_ptr<Filter> filter;
  filter.reset(&params.release()->filter);
  Action::ActionType action_type = Action::ACTION_API_CALL;
  switch (filter->activity_type) {
    case Filter::ACTIVITY_TYPE_API_CALL:
      action_type = Action::ACTION_API_CALL;
      break;
    case Filter::ACTIVITY_TYPE_API_EVENT:
      action_type = Action::ACTION_API_EVENT;
      break;
    case Filter::ACTIVITY_TYPE_CONTENT_SCRIPT:
      action_type = Action::ACTION_CONTENT_SCRIPT;
      break;
    case Filter::ACTIVITY_TYPE_DOM_ACCESS:
      action_type = Action::ACTION_DOM_ACCESS;
      break;
    case Filter::ACTIVITY_TYPE_DOM_EVENT:
      action_type = Action::ACTION_DOM_EVENT;
      break;
    case  Filter::ACTIVITY_TYPE_WEB_REQUEST:
      action_type = Action::ACTION_WEB_REQUEST;
      break;
    case Filter::ACTIVITY_TYPE_ANY:
    default:
      action_type = Action::ACTION_ANY;
  }
  std::string extension_id =
      filter->extension_id.get() ? *filter->extension_id : std::string();
  std::string api_call =
      filter->api_call.get() ? *filter->api_call : std::string();
  std::string page_url =
      filter->page_url.get() ? *filter->page_url : std::string();
  std::string arg_url =
      filter->arg_url.get() ? *filter->arg_url : std::string();

  // Call the ActivityLog.
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_);
  DCHECK(activity_log);
  activity_log->GetFilteredActions(
      extension_id,
      action_type,
      api_call,
      page_url,
      arg_url,
      base::Bind(
          &ActivityLogPrivateGetExtensionActivitiesFunction::OnLookupCompleted,
          this));

  return true;
}

void ActivityLogPrivateGetExtensionActivitiesFunction::OnLookupCompleted(
    scoped_ptr<std::vector<scoped_refptr<Action> > > activities) {
  // Convert Actions to ExtensionActivities.
  std::vector<linked_ptr<ExtensionActivity> > result_arr;
  for (std::vector<scoped_refptr<Action> >::iterator it = activities->begin();
       it != activities->end();
       ++it) {
    result_arr.push_back(
        make_linked_ptr(it->get()->ConvertToExtensionActivity().release()));
  }

  // Populate the return object.
  scoped_ptr<ActivityResultSet> result_set(new ActivityResultSet);
  result_set->activities = result_arr;
  results_ = activity_log_private::GetExtensionActivities::Results::Create(
      *result_set);

  SendResponse(true);
}

bool ActivityLogPrivateDeleteDatabaseFunction::RunImpl() {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_);
  DCHECK(activity_log);
  activity_log->DeleteDatabase();
  return true;
}

bool ActivityLogPrivateDeleteUrlsFunction::RunImpl() {
  scoped_ptr<activity_log_private::DeleteUrls::Params> params(
      activity_log_private::DeleteUrls::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Put the arguments in the right format.
  std::vector<GURL> gurls;
  std::vector<std::string> urls = *params->urls.get();
  for (std::vector<std::string>::iterator it = urls.begin();
       it != urls.end();
       ++it) {
    gurls.push_back(GURL(*it));
  }

  ActivityLog* activity_log = ActivityLog::GetInstance(profile_);
  DCHECK(activity_log);
  activity_log->RemoveURLs(gurls);
  return true;
}

}  // namespace extensions

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/extension_webnavigation_api.h"

#include "base/json/json_writer.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_webnavigation_api_constants.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"

namespace keys = extension_webnavigation_api_constants;

// static
ExtensionWebNavigationEventRouter*
ExtensionWebNavigationEventRouter::GetInstance() {
  return Singleton<ExtensionWebNavigationEventRouter>::get();
}

void ExtensionWebNavigationEventRouter::Init() {
  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   NotificationType::NAV_ENTRY_COMMITTED,
                   NotificationService::AllSources());
  }
}

void ExtensionWebNavigationEventRouter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::NAV_ENTRY_COMMITTED:
      NavEntryCommitted(
          Source<NavigationController>(source).ptr(),
          Details<NavigationController::LoadCommittedDetails>(details).ptr());
      break;

    default:
      NOTREACHED();
  }
}

void ExtensionWebNavigationEventRouter::NavEntryCommitted(
    NavigationController* controller,
    NavigationController::LoadCommittedDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey,
                  details->entry->url().spec());
  dict->SetInteger(keys::kFrameIdKey,
                   details->is_main_frame ? 0 : details->entry->page_id());
  dict->SetString(keys::kTransitionTypeKey,
                  PageTransition::CoreTransitionString(
                      details->entry->transition_type()));
  dict->SetString(keys::kTransitionQualifiersKey,
                  PageTransition::QualifierString(
                      details->entry->transition_type()));
  dict->SetReal(keys::kTimeStampKey, base::Time::Now().ToDoubleT());
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnCommitted, json_args);
}

void ExtensionWebNavigationEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    const std::string& json_args) {
  if (profile && profile->GetExtensionMessageService()) {
    profile->GetExtensionMessageService()->DispatchEventToRenderers(
        event_name, json_args, profile, GURL());
  }
}

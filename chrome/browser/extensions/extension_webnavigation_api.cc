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
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "net/base/net_errors.h"

namespace keys = extension_webnavigation_api_constants;

// static
ExtensionWebNavigationEventRouter*
ExtensionWebNavigationEventRouter::GetInstance() {
  return Singleton<ExtensionWebNavigationEventRouter>::get();
}

void ExtensionWebNavigationEventRouter::Init() {
  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   NotificationType::FRAME_PROVISIONAL_LOAD_START,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::FRAME_PROVISIONAL_LOAD_COMMITTED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                   NotificationService::AllSources());
  }
}

void ExtensionWebNavigationEventRouter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::FRAME_PROVISIONAL_LOAD_START:
      FrameProvisionalLoadStart(
          Source<NavigationController>(source).ptr(),
          Details<ProvisionalLoadDetails>(details).ptr());
      break;
    case NotificationType::FRAME_PROVISIONAL_LOAD_COMMITTED:
      FrameProvisionalLoadCommitted(
          Source<NavigationController>(source).ptr(),
          Details<ProvisionalLoadDetails>(details).ptr());
      break;
    case NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR:
      FailProvisionalLoadWithError(
          Source<NavigationController>(source).ptr(),
          Details<ProvisionalLoadDetails>(details).ptr());
      break;

    default:
      NOTREACHED();
  }
}
void ExtensionWebNavigationEventRouter::FrameProvisionalLoadStart(
    NavigationController* controller,
    ProvisionalLoadDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey,
                  details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, 0);
  dict->SetInteger(keys::kRequestIdKey, 0);
  dict->SetReal(keys::kTimeStampKey,
      static_cast<double>(
          (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnBeforeNavigate, json_args);
}

void ExtensionWebNavigationEventRouter::FrameProvisionalLoadCommitted(
    NavigationController* controller,
    ProvisionalLoadDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey,
                  details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, 0);
  dict->SetString(keys::kTransitionTypeKey,
                  PageTransition::CoreTransitionString(
                      details->transition_type()));
  dict->SetString(keys::kTransitionQualifiersKey,
                  PageTransition::QualifierString(
                      details->transition_type()));
  dict->SetReal(keys::kTimeStampKey,
      static_cast<double>(
          (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnCommitted, json_args);
}

void ExtensionWebNavigationEventRouter::FailProvisionalLoadWithError(
    NavigationController* controller,
    ProvisionalLoadDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(keys::kTabIdKey,
                   ExtensionTabUtil::GetTabId(controller->tab_contents()));
  dict->SetString(keys::kUrlKey,
                  details->url().spec());
  dict->SetInteger(keys::kFrameIdKey, 0);
  dict->SetString(keys::kErrorKey,
                  std::string(net::ErrorToString(details->error_code())));
  dict->SetReal(keys::kTimeStampKey,
      static_cast<double>(
          (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds()));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(controller->profile(), keys::kOnErrorOccurred, json_args);
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

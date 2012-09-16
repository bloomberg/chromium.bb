// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/rtc_private/rtc_private_api.h"

#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/rtc_private.h"
#include "content/public/browser/notification_service.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using contacts::Contact;
using contacts::Contact_EmailAddress;
using contacts::Contact_PhoneNumber;

namespace extensions {

namespace {

// Launch event name.
const char kOnLaunchEvent[] = "rtcPrivate.onLaunch";
// Web intent data payload mimetype.
const char kMimeTypeJson[] = "application/vnd.chromium.contact";
// Web intent actions.
const char kActivateAction[] = "activate";
const char kChatAction[] = "chat";
const char kVoiceAction[] = "voice";
const char kVideoAction[] = "video";
// Web intent data structure fields.
const char kNameIntentField[] = "name";
const char kPhoneIntentField[] = "phone";
const char kEmailIntentField[] = "email";

// Returns string representation of intent action.
const char* GetLaunchAction(RtcPrivateEventRouter::LaunchAction action) {
  const char* action_str = kActivateAction;
  switch (action) {
    case RtcPrivateEventRouter::LAUNCH_ACTIVATE:
      action_str = kActivateAction;
      break;
    case RtcPrivateEventRouter::LAUNCH_CHAT:
      action_str = kChatAction;
      break;
    case RtcPrivateEventRouter::LAUNCH_VOICE:
      action_str = kVoiceAction;
      break;
    case RtcPrivateEventRouter::LAUNCH_VIDEO:
      action_str = kVideoAction;
      break;
    default:
      NOTREACHED() << "Unknown action " << action;
      break;
  }
  return action_str;
}

// Creates JSON payload string for contact web intent data.
void GetContactIntentData(const Contact& contact,
                          DictionaryValue* dict) {
  // TODO(derat): This might require more name extraction magic than this.
  dict->SetString(kNameIntentField, contact.full_name());

  ListValue* phone_list = new base::ListValue();
  dict->Set(kPhoneIntentField, phone_list);
  for (int i = 0; i < contact.phone_numbers_size(); i++) {
    const Contact_PhoneNumber& phone_number = contact.phone_numbers(i);
    StringValue* value = Value::CreateStringValue(phone_number.number());
    if (phone_number.primary())
      CHECK(phone_list->Insert(0, value));
    else
      phone_list->Append(value);
  }

  ListValue* email_list = new base::ListValue();
  dict->Set(kEmailIntentField, email_list);
  for (int i = 0; i < contact.email_addresses_size(); i++) {
    const Contact_EmailAddress& email_address = contact.email_addresses(i);
    StringValue* value = Value::CreateStringValue(email_address.address());
    if (email_address.primary())
      CHECK(email_list->Insert(0, value));
    else
      email_list->Append(value);
  }
}

}  // namespace

void RtcPrivateEventRouter::DispatchLaunchEvent(
    Profile* profile, LaunchAction action, const Contact* contact) {
  if (action == RtcPrivateEventRouter::LAUNCH_ACTIVATE) {
    extensions::ExtensionSystem::Get(profile)->event_router()->
        DispatchEventToRenderers(
            kOnLaunchEvent,
            scoped_ptr<ListValue>(new ListValue()),
            profile,
            GURL());
  } else {
    DCHECK(contact);
    extensions::api::rtc_private::LaunchData launch_data;
    launch_data.intent.action = GetLaunchAction(action);
    GetContactIntentData(*contact,
                         &launch_data.intent.data.additional_properties);
    launch_data.intent.type = kMimeTypeJson;
    extensions::ExtensionSystem::Get(profile)->event_router()->
        DispatchEventToRenderers(
            kOnLaunchEvent,
            extensions::api::rtc_private::OnLaunch::Create(launch_data),
            profile,
            GURL());
  }
}

}  // namespace extensions

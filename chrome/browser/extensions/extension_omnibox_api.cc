// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_omnibox_api.h"

#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"

namespace events {
const char kOnInputChanged[] = "experimental.omnibox.onInputChanged/";
const char kOnInputEntered[] = "experimental.omnibox.onInputEntered/";
};  // namespace events

// static
bool ExtensionOmniboxEventRouter::OnInputChanged(
    Profile* profile, const std::string& extension_id,
    const std::string& input, int suggest_id) {
  std::string event_name = events::kOnInputChanged + extension_id;
  if (!profile->GetExtensionMessageService()->HasEventListener(event_name))
    return false;

  ListValue args;
  args.Set(0, Value::CreateStringValue(input));
  args.Set(1, Value::CreateIntegerValue(suggest_id));
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  profile->GetExtensionMessageService()->DispatchEventToRenderers(
      event_name, json_args, profile->IsOffTheRecord(), GURL());
  return true;
}

// static
void ExtensionOmniboxEventRouter::OnInputEntered(
    Profile* profile, const std::string& extension_id,
    const std::string& input) {
  std::string event_name = events::kOnInputEntered + extension_id;

  ListValue args;
  args.Set(0, Value::CreateStringValue(input));
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  profile->GetExtensionMessageService()->DispatchEventToRenderers(
      event_name, json_args, profile->IsOffTheRecord(), GURL());
}

bool OmniboxSendSuggestionsFunction::RunImpl() {
  int request_id;
  ListValue* suggestions_value;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &request_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(1, &suggestions_value));

  ExtensionOmniboxSuggestions details(request_id, suggestions_value);
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_OMNIBOX_SUGGESTIONS_READY,
      Source<Profile>(profile_),
      Details<ExtensionOmniboxSuggestions>(&details));

  return true;
}


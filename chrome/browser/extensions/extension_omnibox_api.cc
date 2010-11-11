// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_omnibox_api.h"

#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"

namespace events {
const char kOnInputStarted[] = "experimental.omnibox.onInputStarted";
const char kOnInputChanged[] = "experimental.omnibox.onInputChanged";
const char kOnInputEntered[] = "experimental.omnibox.onInputEntered";
const char kOnInputCancelled[] = "experimental.omnibox.onInputCancelled";
};  // namespace events

namespace {
const char kDescriptionStylesOrderError[] =
    "Suggestion descriptionStyles must be in increasing non-overlapping order.";
const char kDescriptionStylesLengthError[] =
    "Suggestion descriptionStyles contains an offset longer than the"
    " description text";

const char kSuggestionContent[] = "content";
const char kSuggestionDescription[] = "description";
const char kSuggestionDescriptionStyles[] = "descriptionStyles";
const char kDescriptionStylesType[] = "type";
const char kDescriptionStylesOffset[] = "offset";
const char kDescriptionStylesLength[] = "length";
};  // namespace

// static
void ExtensionOmniboxEventRouter::OnInputStarted(
    Profile* profile, const std::string& extension_id) {
  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, events::kOnInputStarted, "[]", profile, GURL());
}

// static
bool ExtensionOmniboxEventRouter::OnInputChanged(
    Profile* profile, const std::string& extension_id,
    const std::string& input, int suggest_id) {
  if (!profile->GetExtensionEventRouter()->ExtensionHasEventListener(
        extension_id, events::kOnInputChanged))
    return false;

  ListValue args;
  args.Set(0, Value::CreateStringValue(input));
  args.Set(1, Value::CreateIntegerValue(suggest_id));
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, events::kOnInputChanged, json_args, profile, GURL());
  return true;
}

// static
void ExtensionOmniboxEventRouter::OnInputEntered(
    Profile* profile, const std::string& extension_id,
    const std::string& input) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(input));
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, events::kOnInputEntered, json_args, profile, GURL());

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_OMNIBOX_INPUT_ENTERED,
      Source<Profile>(profile), NotificationService::NoDetails());
}

// static
void ExtensionOmniboxEventRouter::OnInputCancelled(
    Profile* profile, const std::string& extension_id) {
  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, events::kOnInputCancelled, "[]", profile, GURL());
}

bool OmniboxSendSuggestionsFunction::RunImpl() {
  ExtensionOmniboxSuggestions suggestions;
  ListValue* suggestions_value;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &suggestions.request_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(1, &suggestions_value));

  suggestions.suggestions.resize(suggestions_value->GetSize());
  for (size_t i = 0; i < suggestions_value->GetSize(); ++i) {
    ExtensionOmniboxSuggestion& suggestion = suggestions.suggestions[i];
    DictionaryValue* suggestion_value;
    EXTENSION_FUNCTION_VALIDATE(suggestions_value->GetDictionary(
        i, &suggestion_value));
    EXTENSION_FUNCTION_VALIDATE(suggestion_value->GetString(
        kSuggestionContent, &suggestion.content));
    EXTENSION_FUNCTION_VALIDATE(suggestion_value->GetString(
        kSuggestionDescription, &suggestion.description));

    if (suggestion_value->HasKey(kSuggestionDescriptionStyles)) {
      ListValue* styles;
      EXTENSION_FUNCTION_VALIDATE(
          suggestion_value->GetList(kSuggestionDescriptionStyles, &styles));
      EXTENSION_FUNCTION_VALIDATE(suggestion.ReadStylesFromValue(*styles));
    } else {
      suggestion.description_styles.clear();
      suggestion.description_styles.push_back(
          ACMatchClassification(0, ACMatchClassification::NONE));
    }
  }

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_OMNIBOX_SUGGESTIONS_READY,
      Source<Profile>(profile_),
      Details<ExtensionOmniboxSuggestions>(&suggestions));

  return true;
}

ExtensionOmniboxSuggestion::ExtensionOmniboxSuggestion() {}

ExtensionOmniboxSuggestion::~ExtensionOmniboxSuggestion() {}

bool ExtensionOmniboxSuggestion::ReadStylesFromValue(
    const ListValue& styles_value) {
  // Start with a NONE style covering the entire range. As we iterate over the
  // styles, bisect or overwrite the running style list with each new style.
  description_styles.clear();
  description_styles.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));

  for (size_t i = 0; i < styles_value.GetSize(); ++i) {
    DictionaryValue* style;
    std::string type;
    int offset;
    int length;
    if (!styles_value.GetDictionary(i, &style))
      return false;
    if (!style->GetString(kDescriptionStylesType, &type))
      return false;
    if (!style->GetInteger(kDescriptionStylesOffset, &offset))
      return false;
    if (!style->GetInteger(kDescriptionStylesLength, &length))
      return false;

    int type_class =
        (type == "url") ? ACMatchClassification::URL :
        (type == "match") ? ACMatchClassification::MATCH :
        (type == "dim") ? ACMatchClassification::DIM : -1;
    if (type_class == -1)
      return false;

    InsertNewStyle(type_class, offset, length);
  }

  return true;
}

void ExtensionOmniboxSuggestion::InsertNewStyle(int type,
                                                size_t offset,
                                                size_t length) {
  // Find the first and last existing styles that this new style overlaps. Those
  // will have to be removed (if they completely overlap), or bisected.
  size_t start = 0, end = 0;
  while (end < description_styles.size()) {
    if (start < description_styles.size() &&
        description_styles[start].offset < offset)
      ++start;
    if (description_styles[end].offset <= offset + length) {
      ++end;
    } else {
      break;
    }
  }

  DCHECK_GT(end, 0u);
  DCHECK(start == description_styles.size() ||
         description_styles[start].offset >= offset);
  DCHECK(end == description_styles.size() ||
         description_styles[end].offset > offset + length);

  // The last style in the overlapping range will come after our new style.
  int last_style = description_styles[end - 1].style;

  // Remove all overlapping styles.
  if (start < end) {
    description_styles.erase(description_styles.begin() + start,
                             description_styles.begin() + end);
  }

  // Insert our new style.
  description_styles.insert(description_styles.begin() + start,
                            ACMatchClassification(offset, type));
  if (offset + length < description.length()) {
     description_styles.insert(description_styles.begin() + start + 1,
                               ACMatchClassification(offset + length,
                                                     last_style));
  }
}

ExtensionOmniboxSuggestions::ExtensionOmniboxSuggestions() : request_id(0) {}

ExtensionOmniboxSuggestions::~ExtensionOmniboxSuggestions() {}

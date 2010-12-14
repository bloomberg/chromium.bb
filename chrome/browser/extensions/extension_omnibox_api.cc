// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_omnibox_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/notification_service.h"

namespace events {
const char kOnInputStarted[] = "omnibox.onInputStarted";
const char kOnInputChanged[] = "omnibox.onInputChanged";
const char kOnInputEntered[] = "omnibox.onInputEntered";
const char kOnInputCancelled[] = "omnibox.onInputCancelled";
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

static base::LazyInstance<PropertyAccessor<ExtensionOmniboxSuggestion> >
    g_extension_omnibox_suggestion_property_accessor(base::LINKER_INITIALIZED);

PropertyAccessor<ExtensionOmniboxSuggestion>& GetPropertyAccessor() {
  return g_extension_omnibox_suggestion_property_accessor.Get();
}

// Returns the suggestion object set by the extension via the
// omnibox.setDefaultSuggestion call, or NULL if it was never set.
const ExtensionOmniboxSuggestion* GetDefaultSuggestionForExtension(
    Profile* profile, const std::string& extension_id) {
  const Extension* extension =
      profile->GetExtensionService()->GetExtensionById(extension_id, false);
  if (!extension)
    return NULL;
  return GetPropertyAccessor().GetProperty(
      profile->GetExtensionService()->GetPropertyBag(extension));
}

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

bool OmniboxSetDefaultSuggestionFunction::RunImpl() {
  ExtensionOmniboxSuggestion suggestion;
  DictionaryValue* suggestion_value;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &suggestion_value));
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

  // Store the suggestion in the extension's runtime data.
  GetPropertyAccessor().SetProperty(
      profile_->GetExtensionService()->GetPropertyBag(GetExtension()),
      suggestion);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_OMNIBOX_DEFAULT_SUGGESTION_CHANGED,
      Source<Profile>(profile_),
      NotificationService::NoDetails());

  return true;
}

ExtensionOmniboxSuggestion::ExtensionOmniboxSuggestion() {}

ExtensionOmniboxSuggestion::~ExtensionOmniboxSuggestion() {}

bool ExtensionOmniboxSuggestion::ReadStylesFromValue(
    const ListValue& styles_value) {
  description_styles.clear();

  // Step 1: Build a vector of styles, 1 per character of description text.
  std::vector<int> styles;
  styles.resize(description.length());  // sets all styles to 0

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
    if (!style->GetInteger(kDescriptionStylesLength, &length) || length < 0)
      length = description.length();

    if (offset < 0)
      offset = std::max(0, static_cast<int>(description.length()) + offset);

    int type_class =
        (type == "url") ? ACMatchClassification::URL :
        (type == "match") ? ACMatchClassification::MATCH :
        (type == "dim") ? ACMatchClassification::DIM : -1;
    if (type_class == -1)
      return false;

    for (int j = offset;
         j < offset + length && j < static_cast<int>(styles.size()); ++j)
      styles[j] |= type_class;
  }

  // Step 2: Convert the vector into continuous runs of common styles.
  for (size_t i = 0; i < styles.size(); ++i) {
    if (i == 0 || styles[i] != styles[i-1])
      description_styles.push_back(ACMatchClassification(i, styles[i]));
  }

  return true;
}

ExtensionOmniboxSuggestions::ExtensionOmniboxSuggestions() : request_id(0) {}

ExtensionOmniboxSuggestions::~ExtensionOmniboxSuggestions() {}

void ApplyDefaultSuggestionForExtensionKeyword(
    Profile* profile,
    const TemplateURL* keyword,
    const string16& remaining_input,
    AutocompleteMatch* match) {
  DCHECK(keyword->IsExtensionKeyword());
  const ExtensionOmniboxSuggestion* suggestion =
      GetDefaultSuggestionForExtension(profile, keyword->GetExtensionId());
  if (!suggestion)
    return;  // fall back to the universal default

  const string16 kPlaceholderText(ASCIIToUTF16("%s"));
  const string16 kReplacementText(ASCIIToUTF16("<input>"));

  string16 description = suggestion->description;
  ACMatchClassifications& description_styles = match->contents_class;
  description_styles = suggestion->description_styles;

  // Replace "%s" with the user's input and adjust the style offsets to the
  // new length of the description.
  size_t placeholder(suggestion->description.find(kPlaceholderText, 0));
  if (placeholder != string16::npos) {
    string16 replacement =
        remaining_input.empty() ? kReplacementText : remaining_input;
    description.replace(placeholder, kPlaceholderText.length(), replacement);

    for (size_t i = 0; i < description_styles.size(); ++i) {
      if (description_styles[i].offset > placeholder)
        description_styles[i].offset += replacement.length() - 2;
    }
  }

  match->contents.assign(UTF16ToWide(description));
}

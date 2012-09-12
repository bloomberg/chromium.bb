// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_service.h"

namespace events {
const char kOnInputStarted[] = "omnibox.onInputStarted";
const char kOnInputChanged[] = "omnibox.onInputChanged";
const char kOnInputEntered[] = "omnibox.onInputEntered";
const char kOnInputCancelled[] = "omnibox.onInputCancelled";
}  // namespace events

namespace extensions {

namespace {
const char kDescriptionStylesOrderError[] =
    "Suggestion descriptionStyles must be in increasing non-overlapping order.";
const char kDescriptionStylesLengthError[] =
    "Suggestion descriptionStyles contains an offset longer than the"
    " description text";

const char kSuggestionContent[] = "content";
const char kSuggestionDescription[] = "description";
const char kSuggestionDescriptionStyles[] = "descriptionStyles";
const char kSuggestionDescriptionStylesRaw[] = "descriptionStylesRaw";
const char kDescriptionStylesType[] = "type";
const char kDescriptionStylesOffset[] = "offset";
const char kDescriptionStylesLength[] = "length";

}  // namespace

// static
void ExtensionOmniboxEventRouter::OnInputStarted(
    Profile* profile, const std::string& extension_id) {
  scoped_ptr<ListValue> event_args(new ListValue());
  profile->GetExtensionEventRouter()->DispatchEventToExtension(extension_id,
      events::kOnInputStarted, event_args.Pass(), profile, GURL());
}

// static
bool ExtensionOmniboxEventRouter::OnInputChanged(
    Profile* profile, const std::string& extension_id,
    const std::string& input, int suggest_id) {
  if (!profile->GetExtensionEventRouter()->ExtensionHasEventListener(
        extension_id, events::kOnInputChanged))
    return false;

  scoped_ptr<ListValue> args(new ListValue());
  args->Set(0, Value::CreateStringValue(input));
  args->Set(1, Value::CreateIntegerValue(suggest_id));

  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, events::kOnInputChanged, args.Pass(), profile, GURL());
  return true;
}

// static
void ExtensionOmniboxEventRouter::OnInputEntered(
    TabContents* tab_contents,
    const std::string& extension_id,
    const std::string& input) {
  Profile* profile = tab_contents->profile();

  const Extension* extension =
      ExtensionSystem::Get(profile)->extension_service()->extensions()->
          GetByID(extension_id);
  CHECK(extension);
  extensions::TabHelper::FromWebContents(tab_contents->web_contents())->
      active_tab_permission_manager()->GrantIfRequested(extension);

  scoped_ptr<ListValue> args(new ListValue());
  args->Set(0, Value::CreateStringValue(input));

  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, events::kOnInputEntered, args.Pass(), profile, GURL());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_OMNIBOX_INPUT_ENTERED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());
}

// static
void ExtensionOmniboxEventRouter::OnInputCancelled(
    Profile* profile, const std::string& extension_id) {
  scoped_ptr<ListValue> args(new ListValue());
  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, events::kOnInputCancelled, args.Pass(), profile, GURL());
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
    EXTENSION_FUNCTION_VALIDATE(suggestion.Populate(*suggestion_value, true));
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_OMNIBOX_SUGGESTIONS_READY,
      content::Source<Profile>(profile_->GetOriginalProfile()),
      content::Details<ExtensionOmniboxSuggestions>(&suggestions));

  return true;
}

bool OmniboxSetDefaultSuggestionFunction::RunImpl() {
  ExtensionOmniboxSuggestion suggestion;
  DictionaryValue* suggestion_value;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &suggestion_value));
  EXTENSION_FUNCTION_VALIDATE(suggestion.Populate(*suggestion_value, false));

  ExtensionPrefs* prefs =
      ExtensionSystem::Get(profile())->extension_service()->extension_prefs();
  if (prefs)
    prefs->SetOmniboxDefaultSuggestion(extension_id(), suggestion);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_OMNIBOX_DEFAULT_SUGGESTION_CHANGED,
      content::Source<Profile>(profile_->GetOriginalProfile()),
      content::NotificationService::NoDetails());

  return true;
}

ExtensionOmniboxSuggestion::ExtensionOmniboxSuggestion() {}

ExtensionOmniboxSuggestion::~ExtensionOmniboxSuggestion() {}

bool ExtensionOmniboxSuggestion::Populate(const base::DictionaryValue& value,
                                          bool require_content) {
  if (!value.GetString(kSuggestionContent, &content) && require_content)
    return false;

  if (!value.GetString(kSuggestionDescription, &description))
    return false;

  description_styles.clear();
  if (value.HasKey(kSuggestionDescriptionStyles)) {
    // This version comes from the extension.
    const ListValue* styles = NULL;
    if (!value.GetList(kSuggestionDescriptionStyles, &styles) ||
        !ReadStylesFromValue(*styles)) {
      return false;
    }
  } else if (value.HasKey(kSuggestionDescriptionStylesRaw)) {
    // This version comes from ToValue(), which we use to persist to disk.
    const ListValue* styles = NULL;
    if (!value.GetList(kSuggestionDescriptionStylesRaw, &styles) ||
        styles->empty()) {
      return false;
    }
    for (size_t i = 0; i < styles->GetSize(); ++i) {
      const base::DictionaryValue* style = NULL;
      int offset, type;
      if (!styles->GetDictionary(i, &style))
        return false;
      if (!style->GetInteger(kDescriptionStylesType, &type))
        return false;
      if (!style->GetInteger(kDescriptionStylesOffset, &offset))
        return false;
      description_styles.push_back(ACMatchClassification(offset, type));
    }
  } else {
    description_styles.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  }

  return true;
}

bool ExtensionOmniboxSuggestion::ReadStylesFromValue(
    const ListValue& styles_value) {
  description_styles.clear();

  // Step 1: Build a vector of styles, 1 per character of description text.
  std::vector<int> styles;
  styles.resize(description.length());  // sets all styles to 0

  for (size_t i = 0; i < styles_value.GetSize(); ++i) {
    const DictionaryValue* style;
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

scoped_ptr<base::DictionaryValue> ExtensionOmniboxSuggestion::ToValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  value->SetString(kSuggestionContent, content);
  value->SetString(kSuggestionDescription, description);

  if (description_styles.size() > 0) {
    base::ListValue* styles_value = new base::ListValue();
    for (size_t i = 0; i < description_styles.size(); ++i) {
      base::DictionaryValue* style = new base::DictionaryValue();
      style->SetInteger(kDescriptionStylesOffset, description_styles[i].offset);
      style->SetInteger(kDescriptionStylesType, description_styles[i].style);
      styles_value->Append(style);
    }

    value->Set(kSuggestionDescriptionStylesRaw, styles_value);
  }

  return value.Pass();
}

ExtensionOmniboxSuggestions::ExtensionOmniboxSuggestions() : request_id(0) {}

ExtensionOmniboxSuggestions::~ExtensionOmniboxSuggestions() {}

void ApplyDefaultSuggestionForExtensionKeyword(
    Profile* profile,
    const TemplateURL* keyword,
    const string16& remaining_input,
    AutocompleteMatch* match) {
  DCHECK(keyword->IsExtensionKeyword());

  ExtensionPrefs* prefs =
      ExtensionSystem::Get(profile)->extension_service()->extension_prefs();
  if (!prefs)
    return;

  ExtensionOmniboxSuggestion suggestion =
      prefs->GetOmniboxDefaultSuggestion(keyword->GetExtensionId());
  if (suggestion.description.empty())
    return;  // fall back to the universal default

  const string16 kPlaceholderText(ASCIIToUTF16("%s"));
  const string16 kReplacementText(ASCIIToUTF16("<input>"));

  string16 description = suggestion.description;
  ACMatchClassifications& description_styles = match->contents_class;
  description_styles = suggestion.description_styles;

  // Replace "%s" with the user's input and adjust the style offsets to the
  // new length of the description.
  size_t placeholder(suggestion.description.find(kPlaceholderText, 0));
  if (placeholder != string16::npos) {
    string16 replacement =
        remaining_input.empty() ? kReplacementText : remaining_input;
    description.replace(placeholder, kPlaceholderText.length(), replacement);

    for (size_t i = 0; i < description_styles.size(); ++i) {
      if (description_styles[i].offset > placeholder)
        description_styles[i].offset += replacement.length() - 2;
    }
  }

  match->contents.assign(description);
}

}  // namespace extensions

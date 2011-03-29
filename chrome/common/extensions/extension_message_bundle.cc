// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_message_bundle.h"

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace errors = extension_manifest_errors;

const char* ExtensionMessageBundle::kContentKey = "content";
const char* ExtensionMessageBundle::kMessageKey = "message";
const char* ExtensionMessageBundle::kPlaceholdersKey = "placeholders";

const char* ExtensionMessageBundle::kPlaceholderBegin = "$";
const char* ExtensionMessageBundle::kPlaceholderEnd = "$";
const char* ExtensionMessageBundle::kMessageBegin = "__MSG_";
const char* ExtensionMessageBundle::kMessageEnd = "__";

// Reserved messages names.
const char* ExtensionMessageBundle::kUILocaleKey = "@@ui_locale";
const char* ExtensionMessageBundle::kBidiDirectionKey = "@@bidi_dir";
const char* ExtensionMessageBundle::kBidiReversedDirectionKey =
    "@@bidi_reversed_dir";
const char* ExtensionMessageBundle::kBidiStartEdgeKey = "@@bidi_start_edge";
const char* ExtensionMessageBundle::kBidiEndEdgeKey = "@@bidi_end_edge";
const char* ExtensionMessageBundle::kExtensionIdKey = "@@extension_id";

// Reserved messages values.
const char* ExtensionMessageBundle::kBidiLeftEdgeValue = "left";
const char* ExtensionMessageBundle::kBidiRightEdgeValue = "right";

// Formats message in case we encounter a bad formed key in the JSON object.
// Returns false and sets |error| to actual error message.
static bool BadKeyMessage(const std::string& name, std::string* error) {
  *error = base::StringPrintf(
      "Name of a key \"%s\" is invalid. Only ASCII [a-z], "
      "[A-Z], [0-9] and \"_\" are allowed.",
      name.c_str());
  return false;
}

// static
ExtensionMessageBundle* ExtensionMessageBundle::Create(
    const CatalogVector& locale_catalogs,
    std::string* error) {
  scoped_ptr<ExtensionMessageBundle> message_bundle(
      new ExtensionMessageBundle);
  if (!message_bundle->Init(locale_catalogs, error))
    return NULL;

  return message_bundle.release();
}

bool ExtensionMessageBundle::Init(const CatalogVector& locale_catalogs,
                                  std::string* error) {
  dictionary_.clear();

  for (CatalogVector::const_reverse_iterator it = locale_catalogs.rbegin();
       it != locale_catalogs.rend(); ++it) {
    DictionaryValue* catalog = (*it).get();
    for (DictionaryValue::key_iterator key_it = catalog->begin_keys();
         key_it != catalog->end_keys(); ++key_it) {
      std::string key(StringToLowerASCII(*key_it));
      if (!IsValidName(*key_it))
        return BadKeyMessage(key, error);
      std::string value;
      if (!GetMessageValue(*key_it, *catalog, &value, error))
        return false;
      // Keys are not case-sensitive.
      dictionary_[key] = value;
    }
  }

  if (!AppendReservedMessagesForLocale(
      extension_l10n_util::CurrentLocaleOrDefault(), error))
    return false;

  return true;
}

bool ExtensionMessageBundle::AppendReservedMessagesForLocale(
    const std::string& app_locale, std::string* error) {
  SubstitutionMap append_messages;
  append_messages[kUILocaleKey] = app_locale;

  // Calling base::i18n::GetTextDirection on non-UI threads doesn't seems safe,
  // so we use GetTextDirectionForLocale instead.
  if (base::i18n::GetTextDirectionForLocale(app_locale.c_str()) ==
      base::i18n::RIGHT_TO_LEFT) {
    append_messages[kBidiDirectionKey] = "rtl";
    append_messages[kBidiReversedDirectionKey] = "ltr";
    append_messages[kBidiStartEdgeKey] = kBidiRightEdgeValue;
    append_messages[kBidiEndEdgeKey] = kBidiLeftEdgeValue;
  } else {
    append_messages[kBidiDirectionKey] = "ltr";
    append_messages[kBidiReversedDirectionKey] = "rtl";
    append_messages[kBidiStartEdgeKey] = kBidiLeftEdgeValue;
    append_messages[kBidiEndEdgeKey] = kBidiRightEdgeValue;
  }

  // Add all reserved messages to the dictionary, but check for collisions.
  SubstitutionMap::iterator it = append_messages.begin();
  for (; it != append_messages.end(); ++it) {
    if (ContainsKey(dictionary_, it->first)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(
          errors::kReservedMessageFound, it->first);
      return false;
    } else {
      dictionary_[it->first] = it->second;
    }
  }

  return true;
}

bool ExtensionMessageBundle::GetMessageValue(const std::string& key,
                                             const DictionaryValue& catalog,
                                             std::string* value,
                                             std::string* error) const {
  // Get the top level tree for given key (name part).
  DictionaryValue* name_tree;
  if (!catalog.GetDictionaryWithoutPathExpansion(key, &name_tree)) {
    *error = base::StringPrintf("Not a valid tree for key %s.", key.c_str());
    return false;
  }
  // Extract message from it.
  if (!name_tree->GetString(kMessageKey, value)) {
    *error = base::StringPrintf(
        "There is no \"%s\" element for key %s.", kMessageKey, key.c_str());
    return false;
  }

  SubstitutionMap placeholders;
  if (!GetPlaceholders(*name_tree, key, &placeholders, error))
    return false;

  if (!ReplacePlaceholders(placeholders, value, error))
    return false;

  return true;
}

ExtensionMessageBundle::ExtensionMessageBundle() {
}

bool ExtensionMessageBundle::GetPlaceholders(const DictionaryValue& name_tree,
                                             const std::string& name_key,
                                             SubstitutionMap* placeholders,
                                             std::string* error) const {
  if (!name_tree.HasKey(kPlaceholdersKey))
    return true;

  DictionaryValue* placeholders_tree;
  if (!name_tree.GetDictionary(kPlaceholdersKey, &placeholders_tree)) {
    *error = base::StringPrintf("Not a valid \"%s\" element for key %s.",
                                kPlaceholdersKey, name_key.c_str());
    return false;
  }

  for (DictionaryValue::key_iterator key_it = placeholders_tree->begin_keys();
       key_it != placeholders_tree->end_keys(); ++key_it) {
    DictionaryValue* placeholder;
    const std::string& content_key(*key_it);
    if (!IsValidName(content_key))
      return BadKeyMessage(content_key, error);
    if (!placeholders_tree->GetDictionaryWithoutPathExpansion(content_key,
                                                              &placeholder)) {
      *error = base::StringPrintf("Invalid placeholder %s for key %s",
                                  content_key.c_str(),
                                  name_key.c_str());
      return false;
    }
    std::string content;
    if (!placeholder->GetString(kContentKey, &content)) {
      *error = base::StringPrintf("Invalid \"%s\" element for key %s.",
                                  kContentKey, name_key.c_str());
      return false;
    }
    (*placeholders)[StringToLowerASCII(content_key)] = content;
  }

  return true;
}

bool ExtensionMessageBundle::ReplacePlaceholders(
    const SubstitutionMap& placeholders,
    std::string* message,
    std::string* error) const {
  return ReplaceVariables(placeholders,
                          kPlaceholderBegin,
                          kPlaceholderEnd,
                          message,
                          error);
}

bool ExtensionMessageBundle::ReplaceMessages(std::string* text,
                                             std::string* error) const {
  return ReplaceMessagesWithExternalDictionary(dictionary_, text, error);
}

ExtensionMessageBundle::~ExtensionMessageBundle() {
}

// static
bool ExtensionMessageBundle::ReplaceMessagesWithExternalDictionary(
    const SubstitutionMap& dictionary, std::string* text, std::string* error) {
  return ReplaceVariables(dictionary, kMessageBegin, kMessageEnd, text, error);
}

// static
bool ExtensionMessageBundle::ReplaceVariables(
    const SubstitutionMap& variables,
    const std::string& var_begin_delimiter,
    const std::string& var_end_delimiter,
    std::string* message,
    std::string* error) {
  std::string::size_type beg_index = 0;
  const std::string::size_type var_begin_delimiter_size =
    var_begin_delimiter.size();
  while (true) {
    beg_index = message->find(var_begin_delimiter, beg_index);
    if (beg_index == message->npos)
      return true;

    // Advance it immediately to the begining of possible variable name.
    beg_index += var_begin_delimiter_size;
    if (beg_index >= message->size())
      return true;
    std::string::size_type end_index =
      message->find(var_end_delimiter, beg_index);
    if (end_index == message->npos)
      return true;

    // Looking for 1 in substring of ...$1$....
    const std::string& var_name =
      message->substr(beg_index, end_index - beg_index);
    if (!IsValidName(var_name))
      continue;
    SubstitutionMap::const_iterator it =
      variables.find(StringToLowerASCII(var_name));
    if (it == variables.end()) {
      *error = base::StringPrintf("Variable %s%s%s used but not defined.",
                                  var_begin_delimiter.c_str(),
                                  var_name.c_str(),
                                  var_end_delimiter.c_str());
      return false;
    }

    // Replace variable with its value.
    std::string value = it->second;
    message->replace(beg_index - var_begin_delimiter_size,
                     end_index - beg_index + var_begin_delimiter_size +
                       var_end_delimiter.size(),
                     value);

    // And position pointer to after the replacement.
    beg_index += value.size() - var_begin_delimiter_size;
  }

  return true;
}

// static
bool ExtensionMessageBundle::IsValidName(const std::string& name) {
  if (name.empty())
    return false;

  std::string::const_iterator it = name.begin();
  for (; it != name.end(); ++it) {
    // Allow only ascii 0-9, a-z, A-Z, and _ in the name.
    if (!IsAsciiAlpha(*it) && !IsAsciiDigit(*it) && *it != '_' && *it != '@')
      return false;
  }

  return true;
}

// Dictionary interface.

std::string ExtensionMessageBundle::GetL10nMessage(
    const std::string& name) const {
  return GetL10nMessage(name, dictionary_);
}

// static
std::string ExtensionMessageBundle::GetL10nMessage(
    const std::string& name, const SubstitutionMap& dictionary) {
  SubstitutionMap::const_iterator it =
    dictionary.find(StringToLowerASCII(name));
  if (it != dictionary.end()) {
    return it->second;
  }

  return "";
}

///////////////////////////////////////////////////////////////////////////////
//
// Renderer helper functions.
//
///////////////////////////////////////////////////////////////////////////////

// Unique class for Singleton.
struct ExtensionToMessagesMap {
  ExtensionToMessagesMap();
  ~ExtensionToMessagesMap();

  // Maps extension ID to message map.
  ExtensionToL10nMessagesMap messages_map;
};

static base::LazyInstance<ExtensionToMessagesMap> g_extension_to_messages_map(
    base::LINKER_INITIALIZED);

ExtensionToMessagesMap::ExtensionToMessagesMap() {}

ExtensionToMessagesMap::~ExtensionToMessagesMap() {}

ExtensionToL10nMessagesMap* GetExtensionToL10nMessagesMap() {
  return &g_extension_to_messages_map.Get().messages_map;
}

L10nMessagesMap* GetL10nMessagesMap(const std::string& extension_id) {
  ExtensionToL10nMessagesMap::iterator it =
      g_extension_to_messages_map.Get().messages_map.find(extension_id);
  if (it != g_extension_to_messages_map.Get().messages_map.end())
    return &(it->second);

  return NULL;
}

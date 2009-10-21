// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_message_bundle.h"

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"

const wchar_t* ExtensionMessageBundle::kContentKey = L"content";
const wchar_t* ExtensionMessageBundle::kMessageKey = L"message";
const wchar_t* ExtensionMessageBundle::kPlaceholdersKey = L"placeholders";

const char* ExtensionMessageBundle::kPlaceholderBegin = "$";
const char* ExtensionMessageBundle::kPlaceholderEnd = "$";
const char* ExtensionMessageBundle::kMessageBegin = "__MSG_";
const char* ExtensionMessageBundle::kMessageEnd = "__";

const char* ExtensionMessageBundle::kExtensionName = "chrome_extension_name";
const char* ExtensionMessageBundle::kExtensionDescription =
  "chrome_extension_description";

// Formats message in case we encounter a bad formed key in the JSON object.
// Returns false and sets |error| to actual error message.
static bool BadKeyMessage(const std::string& name, std::string* error) {
  *error = StringPrintf("Name of a key \"%s\" is invalid. Only ASCII [a-z], "
                        "[A-Z], [0-9] and \"_\" are allowed.", name.c_str());
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

  CatalogVector::const_reverse_iterator it = locale_catalogs.rbegin();
  for (; it != locale_catalogs.rend(); ++it) {
    DictionaryValue* catalog = (*it).get();
    DictionaryValue::key_iterator key_it = catalog->begin_keys();
    for (; key_it != catalog->end_keys(); ++key_it) {
      std::string key(StringToLowerASCII(WideToUTF8(*key_it)));
      if (!IsValidName(*key_it))
        return BadKeyMessage(key, error);
      std::string value;
      if (!GetMessageValue(*key_it, *catalog, &value, error))
        return false;
      // Keys are not case-sensitive.
      dictionary_[key] = value;
    }
  }

  return true;
}

bool ExtensionMessageBundle::GetMessageValue(const std::wstring& wkey,
                                             const DictionaryValue& catalog,
                                             std::string* value,
                                             std::string* error) const {
  std::string key(WideToUTF8(wkey));
  // Get the top level tree for given key (name part).
  DictionaryValue* name_tree;
  if (!catalog.GetDictionary(wkey, &name_tree)) {
    *error = StringPrintf("Not a valid tree for key %s.", key.c_str());
    return false;
  }
  // Extract message from it.
  if (!name_tree->GetString(kMessageKey, value)) {
    *error = StringPrintf("There is no \"%s\" element for key %s.",
                          WideToUTF8(kMessageKey).c_str(),
                          key.c_str());
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
    *error = StringPrintf("Not a valid \"%s\" element for key %s.",
                          WideToUTF8(kPlaceholdersKey).c_str(),
                          name_key.c_str());
    return false;
  }

  for (DictionaryValue::key_iterator key_it = placeholders_tree->begin_keys();
       key_it != placeholders_tree->end_keys();
       ++key_it) {
    DictionaryValue* placeholder;
    std::string content_key = WideToUTF8(*key_it);
    if (!IsValidName(*key_it))
      return BadKeyMessage(content_key, error);
    if (!placeholders_tree->GetDictionary(*key_it, &placeholder)) {
      *error = StringPrintf("Invalid placeholder %s for key %s",
                            content_key.c_str(),
                            name_key.c_str());
      return false;
    }
    std::string content;
    if (!placeholder->GetString(kContentKey, &content)) {
      *error = StringPrintf("Invalid \"%s\" element for key %s.",
                            WideToUTF8(kContentKey).c_str(),
                            name_key.c_str());
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
  return ReplaceVariables(dictionary_, kMessageBegin, kMessageEnd, text, error);
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
      *error = StringPrintf("Variable %s%s%s used but not defined.",
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
template <typename str>
bool ExtensionMessageBundle::IsValidName(const str& name) {
  if (name.empty())
    return false;

  for (typename str::const_iterator it = name.begin(); it != name.end(); ++it) {
    // Allow only ascii 0-9, a-z, A-Z, and _ in the name.
    if (!IsAsciiAlpha(*it) && !IsAsciiDigit(*it) && *it != '_')
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

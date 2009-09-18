// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUNDLE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUNDLE_H_

#include <string>

#include "base/hash_tables.h"
#include "base/values.h"

// Contains localized extension messages for one locale. Any messages that the
// locale does not provide are pulled from the default locale.
class ExtensionMessageBundle {
 public:
  typedef base::hash_map<std::string, std::string> SubstitutionMap;

  // JSON keys of interest for messages file.
  static const wchar_t* kContentKey;
  static const wchar_t* kMessageKey;
  static const wchar_t* kPlaceholdersKey;

  // Begin/end markers for placeholders and messages
  static const char* kPlaceholderBegin;
  static const char* kPlaceholderEnd;
  static const char* kMessageBegin;
  static const char* kMessageEnd;

  // Extension name and description message names
  static const char* kExtensionName;
  static const char* kExtensionDescription;

  // Creates ExtensionMessageBundle or returns NULL if there was an error.
  static ExtensionMessageBundle* Create(
      const DictionaryValue& default_locale_catalog,
      const DictionaryValue& current_locale_catalog,
      std::string* error);

  // Get message from the catalog with given key.
  // Returned message has all of the internal placeholders resolved to their
  // value (content).
  // Returns empty string if it can't find a message.
  // We don't use simple GetMessage name, since there is a global
  // #define GetMessage GetMessageW override in Chrome code.
  std::string GetL10nMessage(const std::string& name) const;

  // Number of messages in the catalog.
  // Used for unittesting only.
  size_t size() const { return dictionary_.size(); }

  // Replaces all __MSG_message__ with values from the catalog.
  // Returns false if there is a message in text that's not defined in the
  // dictionary.
  bool ReplaceMessages(std::string* text, std::string* error) const;

  // Replaces each occurance of variable placeholder with its value.
  // I.e. replaces __MSG_name__ with value from the catalog with the key "name".
  // Returns false if for a valid message/placeholder name there is no matching
  // replacement.
  // Public for easier unittesting.
  static bool ReplaceVariables(const SubstitutionMap& variables,
                               const std::string& var_begin,
                               const std::string& var_end,
                               std::string* message,
                               std::string* error);

  // Allow only ascii 0-9, a-z, A-Z, and _ in the variable name.
  // Returns false if the input is empty or if it has illegal characters.
  // Public for easier unittesting.
  template<typename str>
  static bool IsValidName(const str& name);

 private:
  // Use Create to create ExtensionMessageBundle instance.
   ExtensionMessageBundle();

  // Initializes the instance from the contents of two catalogs. If a key is not
  // present in current_locale_catalog, the value from default_local_catalog is
  // used instead.
  // Returns false on error.
  bool Init(const DictionaryValue& default_locale_catalog,
            const DictionaryValue& current_locale_catalog,
            std::string* error);

  // Helper methods that navigate JSON tree and return simplified message.
  // They replace all $PLACEHOLDERS$ with their value, and return just key/value
  // of the message.
  bool GetMessageValue(const std::wstring& wkey,
                       const DictionaryValue& catalog,
                       std::string* value,
                       std::string* error) const;

  // Get all placeholders for a given message from JSON subtree.
  bool GetPlaceholders(const DictionaryValue& name_tree,
                       const std::string& name_key,
                       SubstitutionMap* placeholders,
                       std::string* error) const;

  // For a given message, replaces all placeholders with their actual value.
  // Returns false if replacement failed (see ReplaceVariables).
  bool ReplacePlaceholders(const SubstitutionMap& placeholders,
                           std::string* message,
                           std::string* error) const;

  // Holds all messages for application locale.
  SubstitutionMap dictionary_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUNDLE_H_

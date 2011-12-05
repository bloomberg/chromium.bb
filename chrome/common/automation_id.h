// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOMATION_ID_H_
#define CHROME_COMMON_AUTOMATION_ID_H_
#pragma once

#include <string>

namespace base {
class DictionaryValue;
class Value;
}

// A unique ID that JSON automation clients can use to refer to browser
// entities. The ID has a type so that:
// 1) supplying an ID of the wrong type can be detected.
// 2) the client does not have to explicitly supply the type in case multiple
//    ID types can be accepted (e.g., can use a tab ID or extension popup ID for
//    executing javascript).
class AutomationId {
 public:
  // The value of each entry should be preserved.
  enum Type {
    kTypeInvalid = 0,
    kTypeTab,
    kTypeExtensionPopup,
    kTypeExtensionBgPage,
    kTypeExtensionInfobar,
    kTypeExtension,
  };

  static bool FromValue(
      base::Value* value, AutomationId* id, std::string* error);
  static bool FromValueInDictionary(
      base::DictionaryValue* dict, const std::string& key, AutomationId* id,
      std::string* error);

  // Constructs an invalid ID.
  AutomationId();

  // Constructs an ID from the given type and type-specific ID.
  AutomationId(Type type, const std::string& id);

  bool operator==(const AutomationId& id) const;

  // Returns a new dictionary equivalent to this ID.
  base::DictionaryValue* ToValue() const;

  // Returns whether the automation ID is valid.
  bool is_valid() const;

  Type type() const;
  const std::string& id() const;

 private:
  Type type_;
  std::string id_;
};

#endif  // CHROME_COMMON_AUTOMATION_ID_H_

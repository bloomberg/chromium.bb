// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSION_ID_H_
#define CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSION_ID_H_

#include <string>

#include "base/optional.h"

namespace chrome_cleaner {

// A wrapper around std::string which upholds the extension id invariants.
// An extension must be a unique identifier that is 32 characters long between
// 'a' - 'p' for each character.
class ExtensionID {
 public:
  // Creates an ExtensionID if the |value| is a valid extension id.
  // If the extension id is invalid no ExtensionID is stored in the optional.
  static base::Optional<ExtensionID> Create(const std::string& value);
  // Determines if the |value| is a valid extension ID.
  static bool IsValidID(const std::string& value);

  std::string AsString() const;

  bool operator==(const ExtensionID& other) const {
    return value_ == other.value_;
  }

  bool operator<(const ExtensionID& other) const {
    return value_ < other.value_;
  }

  bool operator>(const ExtensionID& other) const {
    return value_ > other.value_;
  }

 private:
  explicit ExtensionID(const std::string& value);

  std::string value_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSION_ID_H_

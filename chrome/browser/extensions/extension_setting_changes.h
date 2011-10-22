// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTING_CHANGES_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTING_CHANGES_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/values.h"

// A list of changes to extension settings.  Create using the Builder class.
// The Changes object is thread-safe and efficient to copy, while the Builder
// object isn't.
class ExtensionSettingChanges {
 public:
  // Non-thread-safe builder for ExtensionSettingChanges.
  class Builder {
   public:
    Builder() {}

    // Appends a change for a setting.
    void AppendChange(
        const std::string& key,
        // Ownership taken.  May be NULL to imply there is no old value.
        Value* old_value,
        // Ownership taken.  May be NULL to imply there is no new value.
        Value* new_value);

    // Creates an ExtensionSettingChanges object.  The Builder should not be
    // used after calling this.
    ExtensionSettingChanges Build();

   private:
    ListValue changes_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  ~ExtensionSettingChanges();

  // Gets the JSON serialized form of the changes, for example:
  // [ { "key": "foo", "oldValue": "bar", "newValue": "baz" } ]
  std::string ToJson() const;

 private:
  // Create using the Builder class.  |changes| will be cleared.
  explicit ExtensionSettingChanges(ListValue* changes);

  // Ref-counted inner class, a wrapper over a non-thread-safe string.
  class Inner : public base::RefCountedThreadSafe<Inner> {
   public:
    Inner() {}

    // swap() this with the changes from the Builder after construction.
    ListValue changes_;

   private:
    virtual ~Inner() {}
    friend class base::RefCountedThreadSafe<Inner>;
  };

  scoped_refptr<Inner> inner_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTING_CHANGES_H_

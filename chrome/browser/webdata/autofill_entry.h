// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__
#define CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__

#include "base/string16.h"

class AutofillKey {
 public:
  AutofillKey(const string16& name, const string16& value)
      : name_(name),
        value_(value) {}
  AutofillKey(const AutofillKey& key)
      : name_(key.name()),
        value_(key.value()) {}
  virtual ~AutofillKey() {}

  const string16& name() const { return name_; }
  const string16& value() const { return value_; }

  bool operator==(const AutofillKey& key) const;

 private:
  string16 name_;
  string16 value_;
};

class AutofillEntry {
 public:
  explicit AutofillEntry(const AutofillKey& key) : key_(key) {}

  const AutofillKey& key() const { return key_; }

 private:
  AutofillKey key_;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__
#define CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__
#pragma once

#include <vector>
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

class AutofillKey {
 public:
  AutofillKey() {}
  AutofillKey(const string16& name, const string16& value)
      : name_(name),
        value_(value) {}
  AutofillKey(const char* name, const char* value)
      : name_(UTF8ToUTF16(name)),
        value_(UTF8ToUTF16(value)) {}
  AutofillKey(const AutofillKey& key)
      : name_(key.name()),
        value_(key.value()) {}
  virtual ~AutofillKey() {}
  const string16& name() const { return name_; }
  const string16& value() const { return value_; }

  bool operator==(const AutofillKey& key) const;
  bool operator<(const AutofillKey& key) const;

 private:
  string16 name_;
  string16 value_;
};

class AutofillEntry {
 public:
  AutofillEntry(const AutofillKey& key,
                const std::vector<base::Time>& timestamps)
      : key_(key),
        timestamps_(timestamps) {}

  const AutofillKey& key() const { return key_; }
  const std::vector<base::Time>& timestamps() const { return timestamps_; }

  bool operator==(const AutofillEntry& entry) const;
  bool operator<(const AutofillEntry& entry) const;

 private:
  AutofillKey key_;
  std::vector<base::Time> timestamps_;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__
#define CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__
#pragma once

#include <stddef.h>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "base/time.h"

class AutofillKey {
 public:
  AutofillKey();
  AutofillKey(const string16& name, const string16& value);
  AutofillKey(const char* name, const char* value);
  AutofillKey(const AutofillKey& key);
  virtual ~AutofillKey();

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
                const std::vector<base::Time>& timestamps);
  ~AutofillEntry();

  const AutofillKey& key() const { return key_; }
  const std::vector<base::Time>& timestamps() const { return timestamps_; }

  bool operator==(const AutofillEntry& entry) const;
  bool operator<(const AutofillEntry& entry) const;

  bool timestamps_culled() const { return timestamps_culled_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillEntryTest, NoCulling);
  FRIEND_TEST_ALL_PREFIXES(AutofillEntryTest, Culling);

  // Culls the list of timestamps to |kMaxAutofillTimeStamps| latest timestamps.
  // Result is stored in |result|. If the original vtor's size is less
  // than kMaxAutofillTimeStamps then false is returned. Otherwise true is
  // returned. Note: source and result should be DIFFERENT vectors.
  static bool CullTimeStamps(const std::vector<base::Time>& source,
                             std::vector<base::Time>* result);

  AutofillKey key_;
  std::vector<base::Time> timestamps_;
  bool timestamps_culled_;
};

// TODO(lipalani): Move this inside the class defintion.
const unsigned int kMaxAutofillTimeStamps = 50;
#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__

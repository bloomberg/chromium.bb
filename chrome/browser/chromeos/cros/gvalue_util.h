// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_GVALUE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROS_GVALUE_UTIL_H_

#include <glib-object.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {

class DictionaryValue;
class Value;

}  // namespace base

namespace chromeos {

// A class to manage GValue resources.
class ScopedGValue {
 public:
  ScopedGValue();
  explicit ScopedGValue(GValue* value);
  ~ScopedGValue();

  // Sets the value.
  void reset(GValue* value);

  // Returns the value.
  GValue* get() {return value_.get();}

 private:
  scoped_ptr<GValue> value_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGValue);
};

// A class to manage GHashTable reference.
class ScopedGHashTable {
 public:
  ScopedGHashTable();
  explicit ScopedGHashTable(GHashTable* table);
  ~ScopedGHashTable();

  // Sets the table.
  void reset(GHashTable* table);

  // Returns the table.
  GHashTable* get() {return table_;}

 private:
  GHashTable* table_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGHashTable);
};

// Converts a Value to a GValue.
GValue* ConvertValueToGValue(const base::Value& value);

// Converts a DictionaryValue to a string-to-value GHashTable.
GHashTable* ConvertDictionaryValueToStringValueGHashTable(
    const base::DictionaryValue& dict);

// Converts a GValue to a Value.
base::Value* ConvertGValueToValue(const GValue* gvalue);

// Converts a string-to-value GHashTable to a DictionaryValue.
base::DictionaryValue* ConvertStringValueGHashTableToDictionaryValue(
    GHashTable* ghash);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_GVALUE_UTIL_H_

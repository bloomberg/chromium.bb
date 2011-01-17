// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INDEXED_DB_KEY_H_
#define CHROME_COMMON_INDEXED_DB_KEY_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"

class IndexedDBKey {
 public:
  IndexedDBKey(); // Defaults to WebKit::WebIDBKey::InvalidType.
  explicit IndexedDBKey(const WebKit::WebIDBKey& key);
  ~IndexedDBKey();

  void SetNull();
  void SetInvalid();
  void SetString(const string16& string);
  void SetDate(double date);
  void SetNumber(double number);
  void Set(const WebKit::WebIDBKey& key);

  WebKit::WebIDBKey::Type type() const { return type_; }
  const string16& string() const { return string_; }
  double date() const { return date_; }
  double number() const { return number_; }

  operator WebKit::WebIDBKey() const;

 private:
  WebKit::WebIDBKey::Type type_;
  string16 string_;
  double date_;
  double number_;
};

#endif  // CHROME_COMMON_INDEXED_DB_KEY_H_

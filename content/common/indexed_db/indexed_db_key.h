// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"

namespace content {

class CONTENT_EXPORT IndexedDBKey {
 public:
  IndexedDBKey(); // Defaults to WebKit::WebIDBKey::InvalidType.
  explicit IndexedDBKey(const WebKit::WebIDBKey& key);
  ~IndexedDBKey();

  void SetNull();
  void SetInvalid();
  void SetArray(const std::vector<IndexedDBKey>& array);
  void SetString(const string16& string);
  void SetDate(double date);
  void SetNumber(double number);
  void Set(const WebKit::WebIDBKey& key);

  WebKit::WebIDBKey::Type type() const { return type_; }
  const std::vector<IndexedDBKey>& array() const { return array_; }
  const string16& string() const { return string_; }
  double date() const { return date_; }
  double number() const { return number_; }

  operator WebKit::WebIDBKey() const;

 private:
  WebKit::WebIDBKey::Type type_;
  std::vector<IndexedDBKey> array_;
  string16 string_;
  double date_;
  double number_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_H_

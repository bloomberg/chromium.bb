// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_PATH_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_PATH_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebIDBKeyPath.h"

namespace content {

class CONTENT_EXPORT IndexedDBKeyPath {
 public:
  IndexedDBKeyPath();  // Defaults to WebKit::WebIDBKeyPath::NullType.
  explicit IndexedDBKeyPath(const WebKit::WebIDBKeyPath& keyPath);
  ~IndexedDBKeyPath();

  void SetNull();
  void SetArray(const std::vector<string16>& array);
  void SetString(const string16& string);
  void Set(const WebKit::WebIDBKeyPath& key);

  bool IsValid() const;

  WebKit::WebIDBKeyPath::Type type() const { return type_; }
  const std::vector<string16>& array() const { return array_; }
  const string16& string() const { return string_; }

  operator WebKit::WebIDBKeyPath() const;

 private:
  WebKit::WebIDBKeyPath::Type type_;
  std::vector<string16> array_;
  string16 string_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_PATH_H_

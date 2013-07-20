// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_PATH_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_PATH_H_

#include <vector>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebIDBTypes.h"

namespace content {

class CONTENT_EXPORT IndexedDBKeyPath {
 public:
  IndexedDBKeyPath();  // Defaults to WebKit::WebIDBKeyPathTypeNull.
  explicit IndexedDBKeyPath(const string16&);
  explicit IndexedDBKeyPath(const std::vector<string16>&);
  ~IndexedDBKeyPath();

  bool IsNull() const { return type_ == WebKit::WebIDBKeyPathTypeNull; }
  bool operator==(const IndexedDBKeyPath& other) const;

  WebKit::WebIDBKeyPathType type() const { return type_; }
  const std::vector<string16>& array() const;
  const string16& string() const;

 private:
  WebKit::WebIDBKeyPathType type_;
  string16 string_;
  std::vector<string16> array_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_PATH_H_

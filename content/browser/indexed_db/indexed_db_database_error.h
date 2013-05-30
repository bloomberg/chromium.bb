// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseError.h"

namespace content {

class IndexedDBDatabaseError : public base::RefCounted<IndexedDBDatabaseError> {
 public:
  static scoped_refptr<IndexedDBDatabaseError> Create(uint16 code,
                                                      const string16& message) {
    // TODO(jsbell): Assert that this is a valid WebIDBDatabaseException code.
    return make_scoped_refptr(new IndexedDBDatabaseError(code, message));
  }
  static scoped_refptr<IndexedDBDatabaseError> Create(
      const WebKit::WebIDBDatabaseError& other) {
    return make_scoped_refptr(
        new IndexedDBDatabaseError(other.code(), other.message()));
  }

  uint16 code() const { return code_; }
  const string16& message() const { return message_; }

 private:
  IndexedDBDatabaseError(uint16 code, const string16& message)
      : code_(code), message_(message) {}
  ~IndexedDBDatabaseError() {}
  friend class base::RefCounted<IndexedDBDatabaseError>;
  const uint16 code_;
  const string16 message_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_

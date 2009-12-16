// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

#include "chrome/browser/webdata/autofill_entry.h"

class AutofillChange {
 public:
  typedef enum {
    ADD,
    UPDATE,
    REMOVE
  } Type;

  AutofillChange(Type type, const AutofillKey& key)
      : type_(type),
        key_(key) {}
  virtual ~AutofillChange() {}

  Type type() const { return type_; }
  const AutofillKey& key() const { return key_; }

  bool operator==(const AutofillChange& change) const {
    return type_ == change.type() && key_ == change.key();
  }

 private:
  Type type_;
  AutofillKey key_;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

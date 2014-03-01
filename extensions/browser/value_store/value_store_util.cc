// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_util.h"

namespace value_store_util {

scoped_ptr<std::string> NewKey(const std::string& key) {
  return make_scoped_ptr(new std::string(key));
}

scoped_ptr<std::string> NoKey() {
  return scoped_ptr<std::string>();
}

scoped_ptr<ValueStore::Error> NoError() {
  return scoped_ptr<ValueStore::Error>();
}

}  // namespace value_store_util

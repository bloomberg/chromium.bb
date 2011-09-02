// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A tool making it easier to create IDs for unit testing.

#ifndef CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_ID_FACTORY_H_
#define CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_ID_FACTORY_H_
#pragma once

#include <string>

#include "base/string_number_conversions.h"
#include "chrome/browser/sync/syncable/syncable_id.h"

namespace browser_sync {

class TestIdFactory {
 public:
  TestIdFactory() : next_value_(1337000) {}
  ~TestIdFactory() {}

  // Get the root ID.
  static syncable::Id root() {
    return syncable::Id();
  }

  // Make an ID from a number.  If the number is zero, return the root ID.
  // If the number is positive, create a server ID based on the value.  If
  // the number is negative, create a local ID based on the value.  This
  // is deterministic, and [FromNumber(X) == FromNumber(Y)] iff [X == Y].
  static syncable::Id FromNumber(int64 value) {
    if (value == 0)
      return root();
    else if (value < 0)
      return syncable::Id::CreateFromClientString(base::Int64ToString(value));
    else
      return syncable::Id::CreateFromServerId(base::Int64ToString(value));
  }

  // Create a local ID from a name.
  static syncable::Id MakeLocal(std::string name) {
    return syncable::Id::CreateFromClientString(std::string("lient ") + name);
  }

  // Create a server ID from a string name.
  static syncable::Id MakeServer(std::string name) {
    return syncable::Id::CreateFromServerId(std::string("erver ") + name);
  }

  // Autogenerate a fresh local ID.
  syncable::Id NewLocalId() {
    return syncable::Id::CreateFromClientString(
        std::string("_auto ") + base::IntToString(-next_value()));
  }

  // Autogenerate a fresh server ID.
  syncable::Id NewServerId() {
    return syncable::Id::CreateFromServerId(
        std::string("_auto ") + base::IntToString(next_value()));
  }

 private:
  int next_value() {
    return next_value_++;
  }
  int next_value_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_ID_FACTORY_H_


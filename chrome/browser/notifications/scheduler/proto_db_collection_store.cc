// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/proto_db_collection_store.h"

namespace notifications {

bool ExactMatchKeyFilter(const std::string& key_to_load,
                         const std::string& current_key) {
  return key_to_load == current_key;
}

}  // namespace notifications

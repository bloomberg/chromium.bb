// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_notifications.h"

namespace history {

URLsModifiedDetails::URLsModifiedDetails() {}

URLsModifiedDetails::~URLsModifiedDetails() {}

URLsDeletedDetails::URLsDeletedDetails()
    : all_history(false),
      expired(false) {
}

URLsDeletedDetails::~URLsDeletedDetails() {}

}  // namespace history

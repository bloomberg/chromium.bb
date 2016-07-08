// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_observer.h"

namespace content {

IndexedDBObserver::IndexedDBObserver(int32_t observer_id)
    : observer_id_(observer_id) {}

IndexedDBObserver::~IndexedDBObserver() {}

}  // namespace content
